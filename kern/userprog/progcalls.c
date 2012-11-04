#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <syscall.h>
#include <thread.h>
#include <curthread.h>

int
sys__exit(int *retval)
{
	//TODO - Now this just ends the thread when its exit is called, I believe
	// that more has to be done here
	thread_exit();
	
	return 0;
}

int
sys_fork(int *retval)
{
	kprintf("fork called!\n");
	
	return 0;
}

int
sys_getpid(int *retval)
{
	kprintf("getpid called!\n");
	
	return 0;
}

int
sys_waitpid(int retval)
{
	kprintf("waitpid called\n");
	
	return 0;
}
