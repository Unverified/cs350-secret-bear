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
	segdef->sd_segsz = 0;
	segdef->sd_npage = 0;
	segdef->sd_flags = 0;
	segdef->sd_offset = 0;
	
	return segdef;
}

struct segdef*
sd_copy(struct segdef *old)
{
	struct segdef* new = sd_create();
	
	new->sd_vbase = old->sd_vbase;
	new->sd_segsz = old->sd_segsz;
	new->sd_npage = old->sd_npage;
	new->sd_flags = old->sd_flags;
	new->sd_offset = old->sd_offset;
	
	return new;
}


struct segdef*
sd_get_by_addr(struct addrspace *as, vaddr_t vbase)
{
	assert(as != NULL);
	
	if(as->as_segments == NULL){
		//no segments have been defined yet, im hoping this is a heap access
		return NULL;
	}
	
	struct array *segs = as->as_segments;
	int i,narr = array_getnum(segs);
	
	if(narr < 0 || narr > 5){
		panic("something probably leaked into your memory again, baka");
	}
	
	
	for(i=0; i<narr; i++){
		struct segdef *segdef = (struct segdef*) array_getguy(segs, i);
		vaddr_t segb = segdef->sd_vbase;
		vaddr_t segt = segb + segdef->sd_segsz;
		
		if(segb <= vbase && vbase <= segt){
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
