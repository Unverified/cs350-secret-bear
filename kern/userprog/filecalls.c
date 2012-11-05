/*
filecalls.c

Implementation of all file related system calls:
	- open
	- close
	- read
	- write

*/

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <syscall.h>
#include <filecalls.h>

int
sys_open(int *retval)
{
	kprintf("open called!");
	
	return 0;
}

int
sys_close(int *retval)
{
	kprintf("close called!");
	
	return 0;
}

int
sys_read(int *retval)
{
	kprintf("read called!");

	return 0;
}

int
sys_write(int fd, const_userptr_t data, size_t len, int *retval)
{
	if(fd < 0){
		*retval = -1;
		return EBADF;
	}
	
	size_t size;
	char *kernspace = (char*) kmalloc(len);
		
	copyinstr(data, kernspace, len, &size);

	//for some reason im getting extra characters on the end of output when I used strings why?
	kprintf("%c", *kernspace);
		
	kfree(kernspace);
	*retval = 0;
	return 0;	
}
