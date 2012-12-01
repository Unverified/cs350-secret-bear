#ifndef VM_VM_TLB_H
#define VM_VM_TLB_H

int tlb_write(vaddr_t faultaddress, u_int32_t dirtybit);
void tlb_invalidate();
void tlb_set_text_read_only(struct addrspace *as);

#endif
