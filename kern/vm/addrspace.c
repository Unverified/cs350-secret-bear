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
	int i, spl, result;
	struct addrspace *as;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;
	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
		//Invalid access throw fault exception
	    case VM_FAULT_READONLY:
			return EFAULT;
		//Valid cases further action required
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
			break;
	    default:
			splx(spl);
			return EINVAL;
	}

	as = curthread->t_vmspace;
	if(as == NULL) return EFAULT;

	paddr_t paddr = pt_get_paddr(curthread->t_pid, faultaddress);
	if(!(paddr & TLBLO_VALID)){
		struct segdef *segdef = sd_get_by_addr(as, faultaddress);
		
		if(segdef != NULL){
			//this is a segment, newly loaded in
			for(i = 0; i < segdef->sd_npage; i++) {
				result = pt_alloc_page(curthread->t_pid, segdef->sd_vbase + i * PAGE_SIZE);
				if(!result) {
					return ENOMEM;
				}
			}
		}else{
			//stack
			/*
			if(faultaddress < as->stackb || faultaddress > as->stackt){
				return EFAULT;
			}
			return EFAULT;
			
			result = pt_alloc_page(curthread->t_pid, faultaddress);
			if(!result) {
				return ENOMEM;
			}
			*/
		}
	}


	result = tlb_write(faultaddress);


	splx(spl);
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
	as->as_elfbin = NULL; //NO ELVES IN THE BIN
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
	int i, nseg=array_getnum(old->as_segments);
	for(i=0;i<nseg; i++){
		struct segdef *oldseg = (struct segdef*) array_getguy(old->as_segments,i);
		struct segdef *newseg = sd_copy(oldseg);
		if(newseg == NULL) return ENOMEM;
	}
	
	
	result = pt_copymem(curthread->t_pid, pid);
	if(result) {
		as_destroy(new);
		return result;
	}

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
	int i, narr = array_getnum(as->as_segments);
	for(i=0; i<narr; i++){
		kfree(array_getguy(as->as_segments, i));
	}
	
	array_destroy(as->as_segments);
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
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	#if OPT_A3
	assert(as != NULL);
	if(vaddr >= USERSTACK){
		return EINVAL;
	}
	
	if(as->as_segments == NULL){
		as->as_segments = array_create();
		array_preallocate(as->as_segments, 2); //we typically use 2 segments right now
	}

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
	segdef->sd_flags = readable | writeable; //we dont deal with executable
	
	int result;
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
as_prepare_load(struct addrspace *as, pid_t pid)
{
	(void)as;
	as->stackt = USERSTACK;
	as->stackb = USERSTACK - STACKPAGES * PAGE_SIZE;
	
	int result,i;
	
	vaddr_t stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
	for(i = 0; i < STACKPAGES; i++) {
		result = pt_alloc_page(pid, stackbase + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

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
