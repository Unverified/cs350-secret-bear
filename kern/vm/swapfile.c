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
	vfs_close(swap_file);
}
