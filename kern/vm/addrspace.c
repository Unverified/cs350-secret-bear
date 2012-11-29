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
	as->as_pbasec = NULL;
	
	as->as_vbased = 0;
	as->as_npaged = 0;
	as->as_pbased = NULL;
	
	as->as_stackpbase = 0;
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

	(void)pid;
	(void)old;

	#if OPT_A3
/*
	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/*if (as_prepare_load(new, pid)) {
		as_destroy(new);
		return ENOMEM;
	}

	assert(new->as_pbase1 != 0);
	assert(new->as_pbase2 != 0);
	assert(new->as_stackpbase != 0);*/

	result = pt_copymem(curthread->t_pid, pid);
	if(result) {
		as_destroy(new);
		return result;
	}

	/*
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		STACKPAGES*PAGE_SIZE);
	*/
	#else
	(void)old;
	#endif
	
	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	#if OPT_A3
	if(as->as_pbasec != NULL) kfree(as->as_pbasec);
	if(as->as_pbased != NULL) kfree(as->as_pbased);
	#endif
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	#if OPT_A3
	if(active_as == as) { return; }
	active_as = as;
	#else
	(void)as;  // suppress warning until code gets written
	#endif
	
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

	/* TODO: Implement better vm */

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
		as->as_pbasec = (paddr_t*) kmalloc(sizeof(paddr_t) * npages);
		
		return 0;
	}

	if (as->as_vbased == 0) {
		as->as_vbased = vaddr;
		as->as_npaged = npages;
		as->as_pbased = (paddr_t*) kmalloc(sizeof(paddr_t) * npages);
		
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;

	#else	
	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	return EUNIMP;

	#endif
}

int
as_prepare_load(struct addrspace *as, pid_t pid)
{
	#if OPT_A3

	as->t_loadingexe = 1;

	/* TODO: Implement better vm */
	assert(as->as_pbasec != NULL);
	assert(as->as_pbased != NULL);
	assert(as->as_stackpbase == 0);

	int i, result;

	for(i = 0; i < as->as_npagec; i++) {
		result = pt_alloc_page(pid, as->as_vbasec + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

	for(i = 0; i < as->as_npagec; i++) {
		as->as_pbasec[i] = pt_get_paddr(pid, as->as_vbasec + i * PAGE_SIZE);
	}

	for(i = 0; i < as->as_npaged; i++) {
		result = pt_alloc_page(pid, as->as_vbased + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

	for(i = 0; i < as->as_npaged; i++) {
		as->as_pbased[i] = pt_get_paddr(pid, as->as_vbased + i * PAGE_SIZE);
	}

	vaddr_t stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
	for(i = 0; i < STACKPAGES; i++) {
		result = pt_alloc_page(pid, stackbase + i * PAGE_SIZE);
		if(!result) {
			return ENOMEM;
		}
	}

	as->as_stackpbase = pt_get_paddr(pid, stackbase);
	
	return 0;

	#else

	(void)as;
	return 0;

	#endif
}

int
as_complete_load(struct addrspace *as)
{
	#if OPT_A3

	as->t_loadingexe = 0;

	tlb_set_text_read_only(as);
	
	return 0;

	#else 

	(void)as;
	return 0;
	
	#endif
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	#if OPT_A3

	/* TODO: Implement better vm */

	assert(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
	
	#else

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;

	#endif
}

