/* page tables and page table entry manipulation */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <synch.h>
#include <vm.h>
#include <pt.h>
#include <coremap.h>
#include <swapfile.h>
#include <vm_tlb.h>
#include <uw-vmstats.h>

struct lock *pt_mutex;
struct page_table_entry *page_table;
int total_pages;

int pt_init(int pages, int coremap_size, paddr_t starting_paddr, vaddr_t coremap_vaddr, struct lock *mutex) {
	int i, pt_size;	
	paddr_t pt_paddr;
	vaddr_t pt_vaddr;	
	total_pages = pages;

	pt_mutex = mutex;

	lock_acquire(pt_mutex);
	
	pt_paddr = starting_paddr + coremap_size * PAGE_SIZE;
	pt_vaddr = PADDR_TO_KVADDR(pt_paddr);
	page_table = (struct page_table_entry *)pt_vaddr;
	pt_size = (sizeof(struct page_table_entry)*total_pages + PAGE_SIZE - 1)/PAGE_SIZE;

	/* Put the coremap pages into to page table */
	for(i=0; i<coremap_size; i++) {
		page_table[i].paddr = starting_paddr + i * PAGE_SIZE;
		page_table[i].vaddr = coremap_vaddr + i * PAGE_SIZE;
		page_table[i].npages = coremap_vaddr - i;
		page_table[i].writeable = 1;
		page_table[i].dirty = 0;
	}

	/* Initialized the paget_table. */
	for(i=coremap_size; i<total_pages; i++) {
		page_table[i].paddr = starting_paddr + i * PAGE_SIZE;
		page_table[i].writeable = 1;
		page_table[i].dirty = 0;
		
		if(i < pt_size + coremap_size) {
			/* Put the page tables pages into to page table */
			page_table[i].vaddr = pt_vaddr + (i - coremap_size) * PAGE_SIZE;
			page_table[i].npages = total_pages - (i - coremap_size);
		} else {
			page_table[i].vaddr = 0;
		}
		
	}

	lock_release(pt_mutex);
	return pt_size;
}

static paddr_t alloc_page(pid_t pid, vaddr_t vaddr, int writeable, int dirty) {
	int page_index;

	page_index = get_free_page();
	
	// out of physical memory
	if(page_index == -1) {
		page_index = get_fifo_page();
		//if(page_table[page_index].dirty){
			swap_out(page_table[page_index].pid, page_table[page_index].paddr, page_table[page_index].vaddr);
		//}
		tlb_invalidate();
	}

	page_table[page_index].vaddr = vaddr;
	page_table[page_index].pid = pid;
	page_table[page_index].npages = 1;
	page_table[page_index].writeable = writeable;
	page_table[page_index].dirty = dirty;

	return page_table[page_index].paddr;
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
				paddr = page_table[i].paddr;
				break;
			}
		}
	}

	lock_release(pt_mutex);

	return paddr;
}

/* Allocates a single page of memory. Currently does not do page replacement. */
paddr_t pt_alloc_page(pid_t pid, vaddr_t vaddr, int writeable, int dirty) {
	paddr_t paddr;

	lock_acquire(pt_mutex);
	paddr = alloc_page(pid, vaddr, writeable, dirty);
	lock_release(pt_mutex);

	return paddr;
}

/* Allocates a chunk of memory for the kernel. It doesn't use page replacement either
 * but i don't know how that is suppose to work since it is expecting one chunk of memory
 * and if we don't have a chunk of memory large enough what are we suppose to replace? */
vaddr_t pt_alloc_kpages(pid_t pid, int npages) {
	int i, page_index;
	paddr_t paddr;
	vaddr_t vaddr;

	/* First we need to check to see if the coremap (also pt) have
	 * initialized. If they have is_coremap_intialized will return 0
	 * and we continue. If coremap has not intialized yet is_coremap_intialized
	 * will use ram_stealmem and return the vaddr. */
	vaddr = is_coremap_initialized(npages);
	if(vaddr) {
		return vaddr;
	}

	lock_acquire(pt_mutex);

	/* Ask for a chunk of mem from coremap */
	page_index = get_free_kpages(npages);
	if(page_index == -1) {
		lock_release(pt_mutex);
		return 0;
	}	

	/* put tose pages into the page table */
	paddr = page_table[page_index].paddr;
	vaddr = PADDR_TO_KVADDR(paddr);
	for(i=0; i<npages; i++) {
		page_table[i + page_index].vaddr = vaddr + i * PAGE_SIZE;
		page_table[i + page_index].pid = pid;
		page_table[i + page_index].npages = npages - i;
	}

	lock_release(pt_mutex);
	return vaddr;
}

/* Copies all paget table entries belonging to curpid to pid */
int pt_copymem(pid_t curpid, pid_t pid) {
	int i;
	paddr_t paddr;

	lock_acquire(pt_mutex);

	for(i = 0; i < total_pages; i++) {
		if(page_table[i].pid == curpid) {
			paddr = alloc_page(pid, page_table[i].vaddr, page_table[i].writeable, page_table[i].dirty);
			if(paddr == 0) {
				lock_release(pt_mutex);
				return ENOMEM;
			}

			memmove((void *)PADDR_TO_KVADDR(paddr),	(const void *)PADDR_TO_KVADDR(page_table[i].paddr), PAGE_SIZE);
		}
	}

	lock_release(pt_mutex);

	return 0;
}

int pt_is_writeable(int pid, vaddr_t vaddr) {
	int i, writeable;

	lock_acquire(pt_mutex);

	for(i = 0; i < total_pages; i++) {
		if(page_table[i].pid == pid) {
			vaddr_t vtop = page_table[i].vaddr + PAGE_SIZE;
			vaddr_t vbottom = page_table[i].vaddr;
			
			if(vaddr >= vbottom && vaddr < vtop) {
				writeable = page_table[i].writeable;
				break;
			}
		}
	}

	lock_release(pt_mutex);

	return writeable;
}

void pt_set_dirty(int pid, vaddr_t vaddr) {
	int i;

	lock_acquire(pt_mutex);

	for(i = 0; i < total_pages; i++) {
		if(page_table[i].pid == pid) {
			vaddr_t vtop = page_table[i].vaddr + PAGE_SIZE;
			vaddr_t vbottom = page_table[i].vaddr;
			
			if(vaddr >= vbottom && vaddr < vtop) {
				page_table[i].dirty = 1;
				break;
			}
		}
	}

	lock_release(pt_mutex);
}

/* 
Finds the index of some PID, vaddr in the page table; called by swap_out 
Returns -1 on error
*/
int pt_search_swap (pid_t pid, vaddr_t va) {
	int i;
	int index = -1;

	lock_acquire(pt_mutex);

        for (i=0; i < total_pages; i++) {
                if ((page_table[i].pid == pid) && (page_table[i].vaddr == va)) {
                        index = i;
                        break;
                }
        }

	lock_release(pt_mutex);

	return index;
}

/* Frees a specific page; called by swap_out */
void pt_free_page_swap (pid_t pid, vaddr_t va) {
	int i;
	int index;
	
	//lock_acquire(pt_mutex);

	for (i=0; i < total_pages; i++) {
		if ((page_table[i].pid == pid) && (page_table[i].vaddr == va)) {
			index = i;
			break;
		}	
	}

	page_table[index].vaddr = 0;
	page_table[index].pid = 0;
	page_table[index].npages = 0;
	free_page(index);

	//lock_release(pt_mutex);
}

void pt_free_pages(pid_t pid) {
	int i;

	//lock_acquire(pt_mutex);

	for(i = 0; i < total_pages; i++) {
		if(page_table[i].pid == pid) {
			page_table[i].vaddr = 0;
			page_table[i].pid = 0;
			page_table[i].npages = 0;
			free_page(i);
		}
	}

	//lock_release(pt_mutex);
}

void pt_free_kpage(vaddr_t vaddr) {
	int i, j;

	for(i = 0; i < total_pages; i++) {
		if(coremap_is_kernel(i)) {
			// vaddr_t vtop = page_table[i].vaddr + PAGE_SIZE;
			// vaddr_t vbottom = page_table[i].vaddr;
			
			if(vaddr == page_table[i].vaddr) {
				int npages = page_table[i].npages;

				for(j=0; j<npages; j++) {
					page_table[i+j].vaddr = 0;
					page_table[i+j].pid = 0;
					page_table[i+j].npages = 0;

					free_page(i+j);
				}
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
/* Used for debuging, delete before submit*/
int pt_entry_count(int kernelOnly) {
	int i, last_i = 0, count = 0;	
	for(i=0; i < total_pages; i++) {
		if((page_table[i].vaddr != 0 && kernelOnly && coremap_is_kernel(i)) ||
		   (page_table[i].vaddr != 0 && !kernelOnly)) {
			count++;
			last_i = i;
		}
	}
	//print_pt_entries(last_i+1);
	return count;
}

void print_pt_entries(int n) {
	int i;	
	for(i=0; i < n; i++) {
		kprintf("pt entry: %d\n", page_table[i].pid);
	}
}





