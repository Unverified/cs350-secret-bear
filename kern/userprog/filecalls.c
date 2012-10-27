#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <syscall.h>

int
sys_open()
{
	
	return 0;
}

int
sys_close()
{
	
	return 0;
}

int
sys_read()
{

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
