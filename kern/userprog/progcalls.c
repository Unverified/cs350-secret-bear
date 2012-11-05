/*
progcalls.c

Implementation of all process related system calls:
	- _exit
	- fork
	- getpid
	- waitpid
*/

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
	
	(void) retval;
	return 0;
}

int
sys_fork(int *retval)
{
	kprintf("fork called!\n");
	
	(void) retval;
	return 0;
}

int
sys_getpid(int *retval)
{
	*retval = curthread->t_pid;
	kprintf("getpid called, pid is %d\n", *retval);
	return 0;
}

int
sys_waitpid(int *retval)
{
	kprintf("waitpid called\n");
	
	(void) retval;
	return 0;
}
