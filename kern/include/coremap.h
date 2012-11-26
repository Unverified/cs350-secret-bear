#ifndef VM_COREMAP_H
#define VM_COREMAP_H

void coremap_bootstrap();
vaddr_t is_coremap_initialized(int n);
int get_free_kpages(int n);
int get_free_page();
int coremap_entry_count();
#endif
