#ifndef VM_SEGMENTS_H
#define VM_SEGMENTS_H
#include <vm.h>

struct addrspace;

struct segdef {
	vaddr_t sd_vbase;
	int sd_npage;
	off_t sd_offset;
	char sd_flags;
};

struct segdef *sd_create(void);
struct segdef *sd_copy(struct segdef *old);
void sd_destroy(struct segdef *segdef);
struct segdef *sd_get_by_addr(struct addrspace *as, vaddr_t vbase);

#endif
