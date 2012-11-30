#ifndef VM_COREMAP_H
#define VM_COREMAP_H

struct coremap_entry {
	int in_use;
	int is_kernel;
	time_t secs;
	u_int32_t nsecs;
};

void coremap_bootstrap();
vaddr_t is_coremap_initialized(int n);
int get_free_kpages(int n);
int get_free_page();
int coremap_entry_count();
void free_page(int page);
int coremap_is_kernel(int index);
int get_fifo_page();
#endif
