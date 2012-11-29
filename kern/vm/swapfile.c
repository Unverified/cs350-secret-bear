/* code for managing and manipulating the swapfile */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <vfs.h>

static struct vnode *swap_file;

void
swap_bootstrap()
{
	int result;
	
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
