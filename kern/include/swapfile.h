/*

Implementation of swap file for when we run out of RAM space for pages

Uses a static array on the kernel heap to track entries that are in swap file

*/

#ifndef VM_SWAPFILE_H
#define VM_SWAPFILE_H

#include <types.h>
#include <array.h>
#include <synch.h>
#include <thread.h>
#include <kern/limits.h>

// For swap_array and tracking swap file stuff 
struct swap_entry {
	struct addrspace * as;	// owning address space
	vaddr_t va;		// corresponding virtual address
	off_t offset; 		// location in swap file
};

/*

// Write selected page table's page to file, then evict entry
// Also updates swap_array
void swap_out ();

// Read some virtual page from swap file and place it in some physical page
// Also modify relative page table entry
void swap_in ();

*/

void swap_bootstrap();
void swap_shutdown();

#endif
