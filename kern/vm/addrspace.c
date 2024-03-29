#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <vm_tlb.h>
#include <thread.h>
#include <curthread.h>
#include <pt.h>
#include <syscall.h>
#include <coremap.h>
#include <array.h>
#include <segments.h>
#include <vnode.h>
#include <vfs.h>
#include <swapfile.h>
#include <uw-vmstats.h>
#include <uio.h>

#include "opt-A3.h"

#include <machine/spl.h>
#include <machine/tlb.h>


/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

static struct addrspace *active_as = NULL;

void
vm_bootstrap(void)
{
	coremap_bootstrap();
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{

	int spl;
	paddr_t addr;

	spl = splhigh();

	addr = pt_alloc_kpages(0, npages);
	
	splx(spl);
	return addr;
}

void 
free_kpages(vaddr_t addr)
{
	pt_free_kpage(addr);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	int spl, result, writeable;
	struct addrspace *as;

	faultaddress &= PAGE_FRAME;
	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	// Set all pages to not be writeable cause we want to know when they are writen to
	switch (faulttype) {
		//Invalid access throw fault exception
	    case VM_FAULT_READONLY:
			writeable = pt_is_writeable(curthread->t_pid, faultaddress);
			if(writeable) {
				pt_set_dirty(curthread->t_pid, faultaddress);
				tlb_update(faultaddress, TLBLO_DIRTY);
				return 0;
			}
			else {
				sys__exit(EFAULT);
				return EFAULT;
			}
		//Valid cases further action required
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
			break;
	    default:
			return EINVAL;
	}

	as = curthread->t_vmspace;
	if(as == NULL) {
		return EFAULT;
	}
	paddr_t paddr = pt_get_paddr(curthread->t_pid, faultaddress);

	char reload = 1;
	if(paddr == 0){
		paddr = swap_in(curthread->t_pid, faultaddress);
		reload = 0;
	}
	
	if(paddr == 0){
		if(faultaddress > USERSTACK){
			panic("The kernel lost something, I think....\n");
		}else{
			struct segdef *segdef = sd_get_by_addr(as, faultaddress);
			
			if(segdef != NULL){
				//loading a new segment into memory
				paddr = pt_alloc_page(curthread->t_pid, faultaddress, (segdef->sd_flags & TLBLO_DIRTY), 0);
				if(!paddr) {
					return ENOMEM;
				}

				//need to be able to write to tlb until completely loaded
				int storeloc;
				
				//Need to ensure nothing happens to this block until it is set up properly
				spl = splhigh();
				result = tlb_write(faultaddress, TLBLO_DIRTY, &storeloc);
				if(result){
					splx(spl);
					return result;
				}
				size_t segsize = (segdef->sd_vbase + segdef->sd_segsz) - faultaddress; //rebase the size off of the current fault spot
				int curpage = (faultaddress - segdef->sd_vbase) / PAGE_SIZE;
				off_t offset = segdef->sd_offset + curpage * PAGE_SIZE;
				
				
				if(segsize > PAGE_SIZE){
					//we will at most load a single page into memory
					segsize = PAGE_SIZE;
				}
			
				//now that space for the segment has been allocated, put the segment in memory
				result = load_segment(as->as_elfbin, offset, faultaddress, PAGE_SIZE,
										segsize, 0);
				if(result) {
					splx(spl);
					return result;
				}
				
				if((segdef->sd_flags & TLBLO_DIRTY) == 0){
					result = tlb_set_read_only(segdef, storeloc);
				}
				splx(spl);
			}else{
				//stack
				if(faultaddress < as->stackb || faultaddress > as->stackt){
					return EFAULT;
				}
				
				paddr = pt_alloc_page(curthread->t_pid, faultaddress, 1, 0);
				if(!paddr) {
					return ENOMEM;
				}

				spl = splhigh(); //dont want to let anyone do anything until block is zeroed
				result = tlb_write(faultaddress, 0, NULL);
				if(result){
					splx(spl);
					return result;
				}
				bzero((userptr_t)faultaddress, PAGE_SIZE);
				splx(spl);
				vmstats_inc(5);
			}		
		}	
	}else{
		//address is in the page table, put back into the TLB
		if(reload){
			vmstats_inc(4); //TLB RELOAD
		}
		result = tlb_write(faultaddress, 0, NULL);
	}
		
	return result;
}
/*************************************************/

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	#if OPT_A3
	as->stackt = 0;
	as->stackb = 0;
	as->as_segments = NULL;
	as->as_elfbin = NULL;
	#endif

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret, pid_t pid)
{
	int result;
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	#if OPT_A3
	new->as_segments = array_create();
	if(new->as_segments==NULL) {
		return ENOMEM;
	}

	int i, nseg=array_getnum(old->as_segments);
	for(i=0;i<nseg; i++){
		struct segdef *oldseg = (struct segdef*) array_getguy(old->as_segments,i);
		struct segdef *newseg = sd_copy(oldseg);
		if(newseg == NULL) return ENOMEM;

		result = array_add(new->as_segments, newseg); 
		if(result) {
			sd_destroy(newseg);
			as_destroy(new);
			return result;
		}
	}

	result = pt_copymem(curthread->t_pid, pid);
	if(result) {
		as_destroy(new);
		return result;
	}

	new->as_elfbin = old->as_elfbin;
	VOP_INCOPEN(new->as_elfbin);
	VOP_INCREF(new->as_elfbin);
	
	#else
	(void)pid;
	(void)old;
	#endif /* OPT_A3 */
	
	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	#if OPT_A3
	if(as->as_segments != NULL){
		int i, narr = array_getnum(as->as_segments);
		for(i=0; i<narr; i++){
			kfree(array_getguy(as->as_segments, i));
		}
		array_destroy(as->as_segments);
	}
	
	if(as->as_elfbin != NULL){
		vfs_close(as->as_elfbin);
		as->as_elfbin = NULL;
	}
	
	#endif
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	#if OPT_A3
	if(active_as == as) return;
	active_as = as;
	#else
	(void)as;  // suppress warning until code gets written
	#endif /* OPT_A3 */
	
	tlb_invalidate();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz, off_t offset,
		 int readable, int writeable, int executable)
{
	#if OPT_A3
	int result, flags = 0;
	assert(as != NULL);
	if(vaddr > USERSTACK){
		return EINVAL;
	}
	
	if(as->as_segments == NULL){
		as->as_segments = array_create();
		if(as->as_segments==NULL) {
			return ENOMEM;
		}

		result = array_preallocate(as->as_segments, 2); //we typically use 2 segments right now
		if(result) {
			return result;
		}
	}

	size_t size = sz;
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
	npages = sz / PAGE_SIZE;

	(void)executable;


	struct segdef *segdef;
	segdef = sd_create();
	if(segdef == NULL) return ENOMEM; 
	segdef->sd_vbase = vaddr;
	segdef->sd_npage = npages;
	segdef->sd_segsz = size;

	if(writeable){
		flags = TLBLO_DIRTY;
	}
	(void) readable;  //Why would I want a memory segment I cant read from?
	(void) executable;//Too cool for executable
	
	segdef->sd_flags = flags;
	segdef->sd_offset = offset;
	
	
	result = array_add(as->as_segments, segdef); 
	if(result) {
		sd_destroy(segdef);
		return result;
	}

	return 0;
	#else	
	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	#endif /* OPT_A3 */
	return EUNIMP;
}

int
as_prepare_load(struct addrspace *as)
{
	as->stackt = USERSTACK;
	as->stackb = USERSTACK - STACKPAGES * PAGE_SIZE;
	
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;	
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	#if OPT_A3
	assert(as != NULL);
	#else
	(void)as;
	#endif /* OPT_A3 */
	
	*stackptr = USERSTACK;
	return 0;
}
