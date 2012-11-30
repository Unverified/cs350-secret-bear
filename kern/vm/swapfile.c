/* code for managing and manipulating the swapfile */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <vfs.h>
#include <swapfile.h>
#include <vm.h>

/* Maximum number of pages in the swap file */
#define SWAP_MAX (SWAP_SIZE/PAGE_SIZE)

static struct vnode *swap_file;
static struct lock * swap_mutex;

// array of pointers to swap_entry s
static struct swap_entry * swap_array[SWAP_MAX];

// Note that each offset should be that of a page size in swap file

// Where to call this?
void
swap_bootstrap()
{
	int result;

	swap_mutex = lock_create("swap_mutex");

	// Set all entries to NULL in swap_array
	int i;
	for (i = 0; i < SWAP_MAX; i++)
	{
		swap_array[i] = NULL;
	}

	// open swap file
//	const char *swapname = "/SWAPFILE";
 	char * swapname = kstrdup("/SWAPFILE");
	result = vfs_open(swapname, O_RDWR | O_CREAT, &swap_file);
	if(result){
		kprintf("Errorcode: %d\n", result);
		kfree(swapname);
		panic("Swap space could not be created, exiting...\n");
	}

	kfree(swapname);	
}

void
swap_shutdown()
{
	// free any remaining swap_array entries
	int i;
	for (i = 0; i < SWAP_MAX; i++)
	{
		if (swap_array[i] != NULL)
			swap_entry_remove(i);
	}

        vfs_close(swap_file);
}

// initialize a new swap entry on the heap
struct swap_entry * swap_entry_init(struct addrspace * as, vaddr_t va, off_t offset) {
	struct swap_entry * result;
	result = kmalloc(sizeof(struct swap_entry));
	
	result->as = as;
	result->va = va;
	result->offset = offset;
	
	return result;
}

// remove single swap entry at some index in swap_array
void swap_entry_remove(int i) {
	kfree(swap_array[i]);
}

// find a certain entry in swap_array
// return -1 when such an entry is not found, normal value between 0 and SWAP_MAX
int swap_search (struct addrspace * as, vaddr_t va) {
	int i;
	int result = -1;

	for (i = 0; i < SWAP_MAX; i++)
	{
		if ((swap_array[i]->as == as) && (swap_array[i]->va == va))
		{
			result = i;
			break;
		}
	}	
	
	return result;
}

// find next free entry in swap_array
// returns -1 if none are found
int swap_find_free() {
	int i;
	int result = -1;

	for (i = 0; i < SWAP_MAX; i++) 
	{
		if (swap_array[i] == NULL)
		{
			result = i;
			break;
		}	
	}

	return result;
}

