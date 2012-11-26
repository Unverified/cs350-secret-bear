#ifndef VM_PT_H
#define VM_PT_H

struct page_table_entry {
	vaddr_t vaddr;
	paddr_t paddr;
	pid_t pid;
	int isKernel;
};

int pt_init(int pages, int coremap_size, paddr_t starting_paddr, vaddr_t coremap_vaddr);
paddr_t pt_get_paddr(pid_t pid, vaddr_t vaddr);
int pt_alloc_page(pid_t pid, vaddr_t vaddr);
vaddr_t pt_alloc_kpages(pid_t pid, int npages);
int pt_entry_count();
void print_pt_entries(int n);

#endif
