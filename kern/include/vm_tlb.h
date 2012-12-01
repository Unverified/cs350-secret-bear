#ifndef VM_VM_TLB_H
#define VM_VM_TLB_H

int tlb_write(vaddr_t faultaddress, int *storeloc);
void tlb_invalidate();
int tlb_set_read_only(struct segdef *as, int storeloc);

#endif
