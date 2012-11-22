/* code for manipulating the TLB (including replacement) */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <uw-vmstats.h>

static unsigned int m_next_victim = 0;

static int tlb_get_rr_victim() {
	int victim;
	victim = m_next_victim;
	m_next_victim = (m_next_victim + 1) % NUM_TLB;
	return victim;
}


static int check_as(struct addrspace *as) {
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	assert(as->as_vbase1 != 0);
	assert(as->as_pbase1 != 0);
	assert(as->as_npages1 != 0);
	assert(as->as_vbase2 != 0);
	assert(as->as_pbase2 != 0);
	assert(as->as_npages2 != 0);
	assert(as->as_stackpbase != 0);
	assert((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	assert((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	assert((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	assert((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	return 0;
}

int tlb_write(vaddr_t faultaddress) {
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	u_int32_t writeable;
	int i, result;
	u_int32_t ehi, elo;
	struct addrspace *as;

	as = curthread->t_vmspace;

	result = check_as(as);
	if(result) {
		return result;
	}

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	writeable = TLBLO_DIRTY;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		if(!as->t_loadingexe)
			writeable = 0;

		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	assert((paddr & PAGE_FRAME)==paddr);

	for (i=0; i<NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		
		//we found a free TLB entry
		vmstats_inc(1);
		break;
	}

	if(i == NUM_TLB) {
		i = tlb_get_rr_victim();

		/*I dont know if we have to do anything special when evicting a TLB entry
		but if do do it here */

		vmstats_inc(2);
	}

	ehi = faultaddress;
	elo = paddr | writeable | TLBLO_VALID;
	TLB_Write(ehi, elo, i);

	vmstats_inc(0);

	return 0;
}

/* 
 * After an executable is loaded in the users address space, all writes 
 * to the text segment should not be allowed 
*/
void tlb_set_text_read_only(struct addrspace *as) {
	vaddr_t vbase1, vtop1;
	paddr_t paddr;
	u_int32_t ehi, elo;
	int i;

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;

	for (i=0; i<NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID && ehi >= vbase1 && ehi < vtop1) {
			// I dont know how to just set the dirty bit to 0 so im doing it the hard way
			paddr = (ehi - vbase1) + as->as_pbase1;
			elo = paddr | TLBLO_VALID;
			TLB_Write(ehi, elo, i);
		}
	}
}

void tlb_invalidate() {
	int i, spl;

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	m_next_victim = 0;

	splx(spl);

	vmstats_inc(3);
}
