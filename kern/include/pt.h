#ifndef VM_PT_H
#define VM_PT_H

#include <types.h>
#include <synch.h>

struct page_table_entry {
	vaddr_t vaddr;
	paddr_t paddr;
	pid_t pid;
	int npages;
	//int writeable;
	//int dirty;
};

int pt_init(int pages, int coremap_size, paddr_t starting_paddr, vaddr_t coremap_vaddr, struct lock *mutex);
paddr_t pt_get_paddr(pid_t pid, vaddr_t vaddr);
paddr_t pt_alloc_page(pid_t pid, vaddr_t vaddr);
vaddr_t pt_alloc_kpages(pid_t pid, int npages);
int pt_copymem(pid_t curpid, pid_t pid);
void pt_free_pages(pid_t pid);
void pt_free_kpage(vaddr_t vaddr);

int pt_entry_count(int kernelOnly);
void print_pt_entries(int n);

#endif
