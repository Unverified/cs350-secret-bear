#ifndef VM_VM_TLB_H
#define VM_VM_TLB_H

int tlb_write(vaddr_t faultaddress, u_int32_t writeable, int *storeloc);
void tlb_update(vaddr_t faultaddress, u_int32_t writeable);
void tlb_invalidate();
void tlb_invalidate_entry(vaddr_t vaddr);
int tlb_set_read_only(struct segdef *as, int storeloc);

#endif
