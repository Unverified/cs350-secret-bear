#ifndef VM_VM_TLB_H
#define VM_VM_TLB_H
#include <segments.h>

int tlb_write(vaddr_t faultaddress, u_int32_t writeable, int *storeloc);
void tlb_update(vaddr_t faultaddress, u_int32_t writeable);
void tlb_invalidate();
int tlb_set_read_only(struct segdef *as, int storeloc);

#endif
