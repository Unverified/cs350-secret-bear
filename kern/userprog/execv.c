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
#include <kern/limits.h>
#include "opt-A2.h"

int
sys_execv(const_userptr_t *progname, userptr_t *args[])
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, nargs, i, freeindex;
	size_t actual;
	char **k_args;
	char *k_progname;
	vaddr_t *addrOfCharPtrs;

	if(progname == NULL || progname >= USERTOP || args == NULL) {
		return EFAULT;
	}

	k_progname = kmalloc(PATH_MAX);
	result = copyinstr(progname,k_progname,PATH_MAX,&actual);	
	if(result) {
		goto fail1;
	} else if (*k_progname == '\0') {
		result = EISDIR;
		goto fail1;
	}

	k_args = kmalloc(sizeof(char*));
	result = copyin(args, k_args, sizeof(char*));
	if(result) {
		goto fail2;
	}

	//get the number of args
	nargs = 0;
	for(i=0; args[i] != NULL; i++) {
		char copytest[1];
		result = copyin(args[i], copytest, 1);
		if(result) {
			freeindex = i-1;
			goto fail3;
		}

		size_t size = strlen(args[i]) + 1;

		char *k_str = kmalloc(size);
		result = copyinstr(args[i], k_str, size, &actual);
		k_args[i] = k_str;

		if(result) {
			freeindex = i;
			goto fail3;
		}
		nargs++;
	}

	freeindex = nargs - 1;

	//need to have atleast the prog name in args
	if(nargs < 1) {
		result = EFAULT;
		goto fail3;
	}

	/* Open the file. */
	result = vfs_open(k_progname, O_RDONLY, &v);
	if (result) {
		goto fail3;
	}

	struct addrspace *prev_as = curthread->t_vmspace;
	struct addrspace *new_as = as_create();
	
	/* Create a new address space. */
	curthread->t_vmspace = new_as;
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		curthread->t_vmspace = prev_as;
		result = ENOMEM;
		goto fail4;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		vfs_close(v);
		goto fail5;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		goto fail5;
	}

	addrOfCharPtrs = kmalloc(sizeof(vaddr_t)*nargs);

	/* First put all the strings args on the users stack  while 
	keeping track of the starting chars user stack address*/
	for(i = nargs - 1; i >= 0; i--) {
		int str_len = strlen(k_args[i]) + 1;
		
		stackptr -= str_len;
		addrOfCharPtrs[i] = stackptr;
		result = copyoutstr(k_args[i], (userptr_t) stackptr, str_len, &actual);
		if(result) {
			goto fail6;
		}
	}


	/* put the 0's on the stack */
	int null_len = 4 + stackptr % 4;
	int zeros = 0;
	stackptr -= null_len;
	result = copyout(&zeros, (userptr_t) stackptr, null_len);
	if(result) {
		goto fail6;
	}

	/* put all the *char on the user stack */
    	for(i = nargs; i >= 0; i--) {
		stackptr -= 4;
		result = copyout(&addrOfCharPtrs[i], (userptr_t) stackptr, 4);
		if(result) {
			goto fail6;
		}
    	}

	/* this is the new **argv address, put that on the user stack */
	vaddr_t newArgsPtr = stackptr;
	stackptr -= 4;
	result = copyout(&newArgsPtr, (userptr_t) stackptr, 4);
	if(result) {
		goto fail6;
	}

	/* make sure the new user stackptr is divisible by 8 */
	stackptr -= (stackptr % 8);
	
	/* clean up the args as they are now in the proper user space */
	for( ; freeindex>=0; freeindex--){
		kfree(k_args[freeindex]);
	}
	kfree(k_args);
	kfree(k_progname);
	kfree(addrOfCharPtrs);
	as_destroy(prev_as);

	/* Warp to user mode. */
	md_usermode(nargs,(userptr_t) newArgsPtr, stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;

    fail6:
	kfree(addrOfCharPtrs);
    fail5:
	curthread->t_vmspace = prev_as;
	as_activate(curthread->t_vmspace);
    fail4:
	as_destroy(new_as);
    fail3:
	for( ; freeindex>=0; freeindex--){
		kfree(k_args[freeindex]);
	}
    fail2:
	kfree(k_args);
    fail1:
	kfree(k_progname);

	return result;
}
