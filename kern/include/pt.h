#ifndef VM_PT_H
#define VM_PT_H

struct page_table_entry {
	vaddr_t vaddr;
	paddr_t paddr;
	pid_t pid;
	int isKernel;
};

void pt_bootstrap();
paddr_t pt_get_paddr(pid_t pid, vaddr_t vaddr);
paddr_t pt_alloc_page(pid_t pid, int page, vaddr_t vbase);
vaddr_t pt_alloc_kpages(pid_t pid, int npages);

#endif
