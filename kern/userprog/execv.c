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

/*int strlen(char *str) {
	int strlen = 0;
	int i;
	for(i = 0; str[i] != '\0'; i++) {
		strlen++;
	}

	return strlen;
}*/

int
sys_execv(char *progname, char **args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	if(progname == NULL || args == NULL) {
		return EINVAL;
	}

	//get the number of args
	int nargs = 0;
	int i;
	for(i=0;;i++) {
		if(args[i] == NULL)
			break;
		nargs++;
	}

	//need to have atleast the prog name in args
	if(nargs < 1) {
		return EINVAL;
	}
	
	//allocate space for the char ptrs on kernel
	char **k_args = (char**)kmalloc(sizeof(char*));

	//put all the args onto kernal space
	for(i=0;i<nargs;i++) {
		int _strlen = strlen(args[i]) + 1;
		char *k_str = kmalloc(sizeof(char)*_strlen);

		size_t actual;
		copyinstr(args[i], k_str, _strlen, &actual);

		k_args[i] = k_str;
	}

	//destroy current user address space
	as_destroy(curthread->t_vmspace);

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

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

	vaddr_t addrOfCharPtrs[nargs+1];

	/* First put all the strings args on the users stack  while 
	keeping track of the starting chars user stack address*/
	for(i = nargs - 1; i >= 0; i--) {
		int str_len = strlen(k_args[i]) + 1;
		
		size_t actual;
		stackptr -= str_len;
		addrOfCharPtrs[i] = stackptr;
		copyoutstr(k_args[i], (userptr_t) stackptr, str_len, &actual);
	}


	/* put the NULL *char on the stack */
	int null_len = 4 - stackptr % 4;
	stackptr -= null_len;
	copyout(NULL, (userptr_t) stackptr, null_len);

	/* put all the *char on the user stack */
    	for(i = nargs; i >= 0; i--) {
		stackptr -= 4;
		copyout(&addrOfCharPtrs[i], (userptr_t) stackptr, 4);
    	}

	/* this is the new **argv address, put that on the user stack */
	vaddr_t newArgsPtr = stackptr;
	stackptr -= 4;
	copyout(&newArgsPtr, (userptr_t) stackptr, 4);

	/* make sure the new user stackptr is divisible by 8 */
	stackptr -= (stackptr % 8);

	kfree(k_args);

	/* Warp to user mode. */
	md_usermode(nargs,(userptr_t) newArgsPtr,
		    stackptr, entrypoint);

	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
