#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <syscall.h>

int sys_open()
{
	
	return 0;
}

int sys_close()
{
	
	return 0;
}

int sys_read()
{

	return 0;
}

int sys_write(int fd, const_userptr_t data, int len, int *retval)
{
	if(fd == 1){
		size_t size;
		char *kernspace = (char*) kmalloc(len * sizeof(char));
		
		
		copyinstr(data, kernspace, len, &size);

		//for some reason im getting extra characters on the end of output from this, why?
		kprintf("%s", kernspace);
		
		kfree(kernspace);
		*retval = 0;
		return 0;	
	}else{
		kprintf("only writes to the stdout, file descriptor == 1 are supported");
		*retval = EBADF; 
		return 1;
	}
}
