#ifndef VM_SEGMENTS_H
#define VM_SEGMENTS_H
#include <vm.h>
#include <kern/types.h>

struct vnode;
struct addrspace;

struct segdef {
	vaddr_t sd_vbase;
	size_t sd_segsz;
	int sd_npage;
	off_t sd_offset;
	char sd_flags;
};

struct segdef *sd_create(void);
struct segdef *sd_copy(struct segdef *old);
void sd_destroy(struct segdef *segdef);
struct segdef *sd_get_by_addr(struct addrspace *as, vaddr_t vbase);

/*part of /userprog/loadelf
 * loads an elf segment into virtual address VADDR.
 * segment starts at offset and is of length filesize
 * this should be called with memsize = PAGE_SIZE
 * also 
 */

int load_segment(struct vnode *v, off_t offset, vaddr_t vaddr, 
	     size_t memsize, size_t filesize, int is_executable);
#endif

