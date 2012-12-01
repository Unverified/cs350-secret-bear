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

struct vnode * swap_file;
struct lock * swap_mutex;

// array of pointers to swap_entry s
struct swap_entry * swap_array[SWAP_MAX];

char * k_data;

// Note that each offset should be that of a page size in swap file

// Where to call this?
void
swap_bootstrap()
{
	int result;

	swap_mutex = lock_create("swap_mutex");
	k_data = kmalloc(sizeof(char)*PAGE_SIZE);

	lock_acquire(swap_mutex);

	// Set all entries to NULL in swap_array
	int i;
	for (i = 0; i < SWAP_MAX; i++)
	{
		swap_array[i] = swap_entry_init(0, 0, 0);
	}

	lock_release(swap_mutex);

	// open swap file
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

	lock_acquire(swap_mutex);

	// free any remaining swap_array entries
	int i;
	for (i = 0; i < SWAP_MAX; i++)
	{
	        kfree(swap_array[i]);	
	}

	kfree(k_data);

	lock_release(swap_mutex);

        vfs_close(swap_file);
}

// read swap file entry and put it into some free physical page 
// Also update page table entry (delete swap entry?)
paddr_t swap_in(pid_t pid, vaddr_t va) {
	int index;

	lock_acquire(swap_mutex);

	index = swap_search(pid, va);

	if (index == -1) {

		lock_release(swap_mutex);
		return 0;
	}

	// get data from swap file and store in k_data
	struct uio uio;

	mk_kuio(&uio, k_data, PAGE_SIZE, swap_array[index]->offset, UIO_READ);	

	int result = VOP_READ(swap_file, &uio);

	// error in read
        if(result){
		lock_release(swap_mutex);
		// hmmm this is a difficult case... we know the page is in the swapfile and we need it to
		// continue, might was to exit the process. 
                panic("Could not read page from swapfile.");
        }

	lock_release(swap_mutex);

	// allocate page of memory
	paddr_t pa;
	pa = pt_alloc_page(swap_array[index]->pid, swap_array[index]->va); 

	// copy memory from holder to physical page
	memmove((void *) PADDR_TO_KVADDR(k_data), (const void *)PADDR_TO_KVADDR(pa), PAGE_SIZE);

	// remove swap table entry
	kfree(swap_array[index]);

	return pa;
}

// write physical page's content to swap file
// evict corresponding page table entry and shoot down TLB entry
// return -1 in case there's no more room
int swap_out(pid_t pid, paddr_t pa, vaddr_t va) {

	// write physical page to swapfile at offset = index * PAGE_SIZE
	struct uio uio;

        // copy from pa to temporary k_data
        memmove((void *) PADDR_TO_KVADDR(pa), (const void *)PADDR_TO_KVADDR(&k_data), PAGE_SIZE);

        lock_acquire(swap_mutex);

	// find free swap file entry
        int index = swap_find_free();

        // no more swap space
        if (index == -1) {

		lock_release(swap_mutex);
                return -1;
        }	
	
	mk_kuio(&uio, k_data, PAGE_SIZE, index*PAGE_SIZE, UIO_WRITE);	
	
	int result = VOP_WRITE(swap_file, &uio);

	// error in write
        if(result){
		lock_release(swap_mutex);
                return result;
        }

	// call evict function which will update page table entry etc
	pt_free_page_swap (pid, va);

	lock_release(swap_mutex);

	return 0;
}

// initialize a new swap entry on the heap
struct swap_entry * swap_entry_init(pid_t pid, vaddr_t va, off_t offset) {
	struct swap_entry * result;
	result = kmalloc(sizeof(struct swap_entry));
	
	result->pid = pid;
	result->va = va;
	result->offset = offset;
	
	return result;
}

// find a certain entry in swap_array
// return -1 when such an entry is not found, normal value between 0 and SWAP_MAX
int swap_search (pid_t pid, vaddr_t va) {
	int i;
	int result = -1;
	for (i = 0; i < SWAP_MAX; i++)
	{
		if ((swap_array[i]->pid == pid) && (swap_array[i]->va == va))
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
		if (swap_array[i]->va == 0)
		{
			result = i;
			break;
		}	
	}

	return result;
}

