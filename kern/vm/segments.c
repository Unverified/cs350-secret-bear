/* code for tracking and manipulating segments */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <segments.h>
#include <addrspace.h>
#include <array.h>

struct segdef*
sd_create()
{
	struct segdef *segdef = kmalloc(sizeof(struct segdef));
		
	segdef->sd_vbase = 0;
	segdef->sd_npage = 0;
	segdef->sd_flags = 0;
	
	return segdef;
}

struct segdef*
sd_copy(struct segdef *old)
{
	struct segdef* new;
	
	new->sd_vbase = old->sd_vbase;
	new->sd_npage = old->sd_npage;
	new->sd_flags = old->sd_flags;
	
	return new;
}


struct segdef*
sd_get_by_addr(struct addrspace *as, vaddr_t vbase)
{
	assert(as != NULL);
	assert(as->as_segments != NULL);
	
	struct array *segs = as->as_segments;
	int i,narr = array_getnum(segs);
	for(i=0; i<narr; i++){
		struct segdef *segdef = (struct segdef*) array_getguy(segs, i);
		if(segdef->sd_vbase == vbase){
			return segdef;
		}
		
	}
	
	return NULL;
}

void
sd_destroy(struct segdef *segdef)
{
	kfree(segdef);
}
