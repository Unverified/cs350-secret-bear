/* keeps track of free physical frames */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <vm.h>
#include <pt.h>
#include <synch.h>
#include <coremap.h>

struct lock *coremap_mutex;
struct coremap_entry *coremap;
int total_pages;
int coremap_init;

static void set_coremap_entry_time(struct coremap_entry *entry) {
	time_t secs;
	u_int32_t nsecs;

	gettime(&secs, &nsecs);

	entry->secs = secs;
	entry->nsecs = nsecs;
}

void coremap_bootstrap() {
	int i, ramsize, coremap_size, pt_size;
	vaddr_t coremap_vaddr;
	u_int32_t lo, hi;
	struct lock *pt_mutex;

	coremap_mutex = lock_create("coremap_mutex");

	/* create the mutex for pt here since we need the lock for pt_init
	 * and lock_create uses kmalloc, so we wan some mem for the pt_mutex
	 * and its easyer if we just grab it before out vm starts booting up */
	pt_mutex = lock_create("pt_mutex");

	lock_acquire(coremap_mutex);

	/* get the amount of ram we have to work with and 
	 * the total number of pages we can have */
	ram_getsize(&lo, &hi);
	ramsize = hi - lo;
	total_pages = ramsize/PAGE_SIZE;

	/* after we call ram_getsize we have to do all the managing. We need
	 * to alloc memory for the page_table but we cant use kmalloc because
	 * we need the page_table for kmallocs. So we need to manually alloc
	 * space for our page_table and assign those pages in the page_table. */
	coremap_size = (sizeof(struct coremap_entry)*total_pages + PAGE_SIZE - 1)/PAGE_SIZE;
	coremap_vaddr = PADDR_TO_KVADDR(lo);
	coremap = (struct coremap_entry *)coremap_vaddr; 

	/* Initialized the coremap, marking coremap pages as being used */
	for(i=0; i<total_pages; i++) {
		if(i < coremap_size) {
			coremap[i].in_use = 1;
			coremap[i].is_kernel = 1;

			set_coremap_entry_time(&coremap[i]);
		} else {
			coremap[i].in_use = 0;
			coremap[i].is_kernel = 0;
		}
		
	}

	pt_size = pt_init(total_pages, coremap_size, lo, coremap_vaddr, pt_mutex);

	/* Assign page tables pages as in use */
	for(i=coremap_size; i<coremap_size+pt_size; i++) {
		coremap[i].in_use = 1;
		coremap[i].is_kernel = 1;

		set_coremap_entry_time(&coremap[i]);
	}

	coremap_init = 1;

	lock_release(coremap_mutex);
}

/* Used to check if the coremap is initialized, if it isnt then return a paddr from ram_stealmem */
vaddr_t is_coremap_initialized(int n) {
	paddr_t paddr = 0;

	if(!coremap_init) {
		paddr = PADDR_TO_KVADDR(ram_stealmem(n));
	}

	return paddr;
}

int get_fifo_page() {
	int i, page_index;
	time_t secs;
	u_int32_t nsecs;

	gettime(&secs, &nsecs);

	for(i = 0; i < total_pages; i++) {
		if(coremap[i].in_use == 1 && coremap[i].is_kernel == 0) {
			if(secs > coremap[i].secs || (secs == coremap[i].secs && nsecs >= coremap[i].nsecs)) {
				page_index = i;
				secs = coremap[i].secs;
				nsecs = coremap[i].nsecs;
			}
		}
	}

	gettime(&secs, &nsecs);

	coremap[page_index].secs = secs;
	coremap[page_index].nsecs = nsecs;

	return page_index;
}

/* Gets a single page of memory */
int get_free_page() {
	int i, page_index;

	lock_acquire(coremap_mutex);

	for(i = 0; i < total_pages; i++) {
		if(coremap[i].in_use == 0) {
			coremap[i].in_use = 1;
			coremap[i].is_kernel = 0;

			set_coremap_entry_time(&coremap[i]);

			page_index = i;
			lock_release(coremap_mutex);
			return page_index;
		}
	}

	lock_release(coremap_mutex);
	return -1;
}

/* Gets a chunk of memory memory */
int get_free_kpages(int n) {
	int i, count, page_index;	

	lock_acquire(coremap_mutex);

	count = 0;
	for(i = 0; i < total_pages; i++) {
		if(coremap[i].in_use == 0) {
			if(count == 0) {
				page_index = i;
			}
			
			count++;

			if(count == n) {
				break;
			}
		} else {
			count = 0;
		}
	}

	if(count < n) {
		// we dont have a chunk of mem big enough
		// dont know what we are suppose to do here.
		lock_release(coremap_mutex);
		return -1; 
	}

	for(i=0; i<n; i++) {
		coremap[i + page_index].in_use = 1;
		coremap[i + page_index].is_kernel = 1;

		set_coremap_entry_time(&coremap[i + page_index]);
	}

	lock_release(coremap_mutex);
	return page_index;
}

void free_page(int page) {
	coremap[page].in_use = 0;
	coremap[page].is_kernel = 0;
}

int coremap_is_kernel(int index) {
	return coremap[index].is_kernel;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/* Used for debuging, delete before submit*/
int coremap_entry_count() {
	int i, count = 0;	
	for(i=0; i < total_pages; i++) {
		if(coremap[i].in_use == 1) {
			//kprintf("coremap[%d]: %p\n", i, &coremap[i]);
			count++;
		}
	}
	return count;
}






