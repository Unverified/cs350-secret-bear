/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include "opt-A2.h"

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, int nargs, char **args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

	#if OPT_A2
	vaddr_t addrOfCharPtrs[nargs];

	/* First put all the strings args on the users stack  while 
	keeping track of the starting chars user stack address*/
	int i;
	for(i = nargs - 1; i >= 0; i--) {
		size_t str_len = 1;
		char *str = args[i];
		int j = 0;
		while(str[j] != '\0') {
			str_len++;
			j++;
		}
		
		size_t actual;
		stackptr -= str_len;
		addrOfCharPtrs[i] = stackptr;
		copyoutstr(args[i], (userptr_t) stackptr, str_len, &actual);		
	}

	/* put the 0's on the stack */
	int null_len = 4 + stackptr % 4;
	int zeros = 0;
	stackptr -= null_len;
	copyout(&zeros, (userptr_t) stackptr, null_len);

	/* put all the *char on the user stack */
    	for(i = nargs-1; i >= 0; i--) {
		stackptr -= 4;
		copyout(&addrOfCharPtrs[i], (userptr_t) stackptr, 4);
    	}

	/* this is the new **argv address, put that on the user stack */
	vaddr_t newArgsPtr = stackptr;
	stackptr -= 4;
	copyout(&newArgsPtr, (userptr_t) stackptr, 4);

	/* make sure the new user stackptr is divisible by 8 */
	stackptr -= (stackptr % 8);

	/* Warp to user mode. */
	md_usermode(nargs,(userptr_t) newArgsPtr,
		    stackptr, entrypoint);
	#else
	/* Warp to user mode. */
	md_usermode(0,NULL,
		    stackptr, entrypoint);
	#endif /* OPT_A2 */
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

