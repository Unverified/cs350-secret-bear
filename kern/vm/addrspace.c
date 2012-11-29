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

#include "opt-A3.h"

#include <machine/spl.h>
#include <machine/tlb.h>


/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *active_as = NULL;

/******** Functions copied from dumb_vm **********/

void
vm_bootstrap(void)
{
	/* Do nothing. */
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
	int spl, result;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;
	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		//probs have to put a diff code
		sys__exit(EFAULT);
		
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
		
	    default:
		splx(spl);
		return EINVAL;
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
	as->as_vbasec = 0;
	as->as_npagec = 0;
	
	as->as_vbased = 0;
	as->as_npaged = 0;
	
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
	new->as_vbasec = old->as_vbasec;
	new->as_npagec = old->as_npagec;
	new->as_vbased = old->as_vbased;
	new->as_npaged = old->as_npaged;

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
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbasec == 0) {
		as->as_vbasec = vaddr;
		as->as_npagec = npages;
		
		return 0;
	}

	if (as->as_vbased == 0) {
		as->as_vbased = vaddr;
		as->as_npaged = npages;
		
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("Warning: too many regions\n");
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
	#if OPT_A3
	as->t_loadingexe = 1;
	int i, result;

	for(i = 0; i < as->as_npagec; i++) {
		result = pt_alloc_page(pid, as->as_vbasec + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

	for(i = 0; i < as->as_npaged; i++) {
		result = pt_alloc_page(pid, as->as_vbased + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

	vaddr_t stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
	for(i = 0; i < STACKPAGES; i++) {
		result = pt_alloc_page(pid, stackbase + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}


	#else
	(void)as;
	#endif /* OPT_A3 */
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	#if OPT_A3
	as->t_loadingexe = 0;
	tlb_set_text_read_only(as);
	#else 
	(void)as;
	#endif /* OPT_A3 */
	
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	#if OPT_A3
	(void)as;
	#else
	(void)as;
	#endif /* OPT_A3 */
	
	*stackptr = USERSTACK;
	return 0;
}

