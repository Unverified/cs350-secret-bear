/* page tables and page table entry manipulation */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <vm.h>
#include <pt.h>
#include <synch.h>

struct lock *pt_mutex;
struct page_table_entry *page_table;
int pt_initialized;
int total_pages;

void pt_bootstrap() {
	int i, ramsize, pt_size;
	vaddr_t pt_vaddr;
	u_int32_t lo, hi;

	pt_mutex = lock_create("pt_mutex");

	lock_acquire(pt_mutex);

	/* get the amount of ram we have to work with and 
	 * the total number of pages we can have */
	ram_getsize(&lo, &hi);
	ramsize = hi - lo;
	total_pages = ramsize/PAGE_SIZE;

	/* after we call ram_getsize we have to do all the managing. We need
	 * to alloc memory for the page_table but we cant use kmalloc because
	 * we need the page_table for kmallocs. So we need to manually alloc
	 * space for our page_table and assign those pages in the page_table. */
	pt_size = (sizeof(struct page_table_entry)*total_pages + PAGE_SIZE)/PAGE_SIZE;
	pt_vaddr = PADDR_TO_KVADDR(lo);
	page_table = pt_vaddr; 

	/* Initialized the paget_table. */
	for(i=0; i<total_pages; i++) {
		page_table[i].paddr = lo + i * PAGE_SIZE;
		
		if(i < pt_size) {
			page_table[i].vaddr = pt_vaddr;
			page_table[i].isKernel = 1;
		} else {
			page_table[i].vaddr = 0;
			page_table[i].isKernel = 0;
		}
		
	}

	pt_initialized = 1;

	lock_release(pt_mutex);
}

/* Gets the physical address from a virtual address for process pid.
 * All it does is looks for page_table_entry's that have a matching pid
 * can looks at the virtual address for each of those pages. If vaddr is
 * in the range of that pages virtual address return that paddr. */
paddr_t pt_get_paddr(pid_t pid, vaddr_t vaddr) {
	int i;
	paddr_t paddr = 0;

	lock_acquire(pt_mutex);

	for(i = 0; i < total_pages; i++) {
		if(page_table[i].pid == pid) {
			vaddr_t vtop = page_table[i].vaddr + PAGE_SIZE;
			vaddr_t vbottom = page_table[i].vaddr;
			
			if(vaddr >= vbottom && vaddr < vtop) {
				u_int32_t offset = vaddr - vbottom;
				paddr = page_table[i].paddr + offset;
				break;
			}
		}
	}

	lock_release(pt_mutex);

	return paddr;
}

/* Allocates a single page of memory. Currently does not do page replacement. */
paddr_t pt_alloc_page(pid_t pid, int page, vaddr_t vbase) {
	int i;

	lock_acquire(pt_mutex);
	
	for(i = 0; i < total_pages; i++) {
		if(page_table[i].vaddr == 0) {
			page_table[i].vaddr = vbase + page * PAGE_SIZE;
			page_table[i].pid = pid;
			page_table[i].isKernel = 0;
			lock_release(pt_mutex);
			return page_table[i].paddr;
		}
	}

	// if we get here we run out of free pages so we need to do page replacement
	// for now just return 0.

	lock_release(pt_mutex);

	return 0;
}

/* Allocates a chunk of memory for the kernel. It doesn't use page replacement either
 * but i don't know how that is suppose to work since it is expecting one chunk of memory
 * and if we don't have a chunk of memory large enough what are we suppose to replace? */
vaddr_t pt_alloc_kpages(pid_t pid, int npages) {
	int i, count, page_index;
	paddr_t paddr;
	vaddr_t vaddr;

	if(!pt_initialized) {
		return PADDR_TO_KVADDR(ram_stealmem(npages));
	}

	lock_acquire(pt_mutex);

	count = 0;
	for(i = 0; i < total_pages; i++) {
		if(page_table[i].vaddr == 0) {
			if(count == 0) {
				paddr = page_table[i].paddr;
				page_index = i;
			}
			
			count++;

			if(count == npages) {
				break;
			}
		} else {
			count = 0;
		}
	}

	if(count < npages) {
		// we dont have a chunk of mem big enough
		// dont know what we are suppose to do here so just return 0 and let kmalloc deal with it.
		lock_release(pt_mutex);
		return 0; 
	}

	vaddr = PADDR_TO_KVADDR(paddr);
	for(i=0; i<npages; i++) {
		page_table[i + page_index].vaddr = vaddr + i * PAGE_SIZE;
		page_table[i + page_index].pid = pid;
		page_table[i + page_index].isKernel = 1;
	}

	lock_release(pt_mutex);

	return vaddr;
}






