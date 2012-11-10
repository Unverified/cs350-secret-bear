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


/*int strlen(char *str) {
	int strlen = 0;
	int i;
	for(i = 0; str[i] != '\0'; i++) {
		strlen++;
	}

	return strlen;
}*/

int
copycheck(const_userptr_t userptr, size_t len, size_t *stoplen)
{
	vaddr_t bot, top;

	*stoplen = len;

	bot = (vaddr_t) userptr;
	top = bot+len-1;

	if (top < bot) {
		/* addresses wrapped around */
		return EFAULT;
	}

	if (bot >= USERTOP) {
		/* region is within the kernel */
		return EFAULT;
	}

	if (top >= USERTOP) {
		/* region overlaps the kernel. adjust the max length. */
		*stoplen = USERTOP - bot;
	}

	return 0;
}

int
sys_execv(const_userptr_t *progname, userptr_t *args[])
{
	//int test = 0;
//kprintf("HERE%d\n", test++);
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	if(progname == NULL || progname >= USERTOP || args == NULL) {
		return EFAULT;
	}
	char *k_progname = kmalloc(PATH_MAX);
	size_t actual;
	result = copyinstr(progname,k_progname,PATH_MAX,&actual);	
	if(result) {
		return result;
	}
	if(*k_progname == '\0') {
		return EISDIR;
	}
	char **k_args;
	k_args = kmalloc(sizeof(char*));
	result = copyin(args, k_args, sizeof(char*));
	if(result) {
		kfree(k_args);
		return EFAULT;
	}

	//get the number of args
	int nargs = 0;
	int i;
	for(i=0;;i++) {
		if(args[i] == NULL)
			break;

		size_t actual;
		char copytest[1];
		result = copyin(args[i], copytest, 1);
		if(result) {
			return result;
		}

		size_t size = strlen(args[i]) + 1;


		char *k_str = kmalloc(size);
		result = copyinstr(args[i], k_str, size, &actual);
		k_args[i] = k_str;

		if(result) {
			for( ; i>=0; i--){
				kfree(k_args[i]);
			}
			return result;
		}
		nargs++;
	}

	//need to have atleast the prog name in args
	if(nargs < 1) {
		//kprintf("HEREerr3\n");
		return EFAULT;
	}
	//put all the args onto kernal space
	/*kprintf("pid: %d %d\n", curthread->t_pid, nargs);
	for(i=0;i<nargs;i++) {
		char test[1];
		result = copyin(k_args[i], test, 1);
		if(result) {
			kprintf("pid: %d err k_args[%d] %p\n", curthread->t_pid, i, k_args[i]);
			for( i=i-1; i>=0; i--){
				kfree(k_args[i]);
			}
			return result;
		}
		
		char *k_str = kmalloc(strlen(k_args[i]));
		size_t actual;
		result = copyinstr(k_args[i], k_str, strlen(k_args[i]), &actual);
		k_args[i] = k_str;
	}*/


	/* Open the file. */
	result = vfs_open(k_progname, O_RDONLY, &v);
	if (result) {
		//kprintf("HEREerr5\n");
		return result;
	}

	struct addrspace *prev_as = curthread->t_vmspace;
	
	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	as_destroy(prev_as);
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
	int null_len = 4 + stackptr % 4;
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
	
	/* clean up the args as they are now in the proper user space */
	for(i=0; i<nargs; i++){
		kfree(k_args[i]);
	}
	kfree(k_args);

	/* Warp to user mode. */
	md_usermode(nargs,(userptr_t) newArgsPtr, stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
