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
#include <pt.h>

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

	return 0;
}

int tlb_write(vaddr_t faultaddress) {
	vaddr_t vbase1, vtop1; //vbase2, vtop2, stackbase, stacktop;
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

/*
	vbase1 = as->as_vbasec;
	vtop1 = vbase1 + as->as_npagec * PAGE_SIZE;
	vbase2 = as->as_vbased;
	vtop2 = vbase2 + as->as_npaged * PAGE_SIZE;
	stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (!(faultaddress >= vbase1 && faultaddress < vtop1) &&
	    !(faultaddress >= vbase2 && faultaddress < vtop2) &&
	    !(faultaddress >= stackbase && faultaddress < stacktop)) {
		return EFAULT;
	}
*/
	paddr = pt_get_paddr(curthread->t_pid, faultaddress);

	if(paddr == 0) {
		// There was a page fault, we need to load the page into memory.
		// This will never happen right now because on demand page loading is
		// not yet implemented, we just load every page into the page table.
		return EFAULT;
	}

	writeable = TLBLO_DIRTY;

	// Do we need this check?; vbase1 and vtop1 are not being defined
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		if(!as->t_loadingexe)
			writeable = 0;

		
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
	/*
	vaddr_t vbase1, vtop1;
	paddr_t paddr;
	u_int32_t ehi, elo;
	int i;

	vbase1 = as->as_vbasec;
	vtop1 = vbase1 + as->as_npagec * PAGE_SIZE;

	for (i=0; i<NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID && ehi >= vbase1 && ehi < vtop1) {
			// Nick for future ref, this is how you drop a flag
			elo &= ~TLBLO_DIRTY;
			
			TLB_Write(ehi, elo, i);
		}
	}
	*/
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
