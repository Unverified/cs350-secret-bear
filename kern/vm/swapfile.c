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

	// open swap file
	char *swapname = "/SWAPFILE";
	result = vfs_open(swapname, O_RDWR | O_CREAT, &swap_file);
	if(result){
		kprintf("Errorcode: %d\n", result);
		panic("Swap space could not be created, exiting...\n");
	}
}


void
swap_shutdown()
{
	vfs_close(swap_file);
}
