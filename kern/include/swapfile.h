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
#include <pt.h>

// For swap_array and tracking swap file stuff 
struct swap_entry {
	vaddr_t va;		// virtual address
	pid_t pid;		// owning pid
	off_t offset; 		// location in swap file
};

// Write selected page table's page to file, then evict entry
// Also updates swap_array
int swap_out (pid_t pid, vaddr_t va);

// Read some virtual page from swap file and place it in some physical page
// Also modify relative page table entry
int swap_in (pid_t pid, vaddr_t va);

// Helper functions
struct swap_entry * swap_entry_init(pid_t pid, vaddr_t va, off_t offset);
int swap_search (pid_t pid, vaddr_t va);
int swap_find_free();

void swap_bootstrap();
void swap_shutdown();

#endif
