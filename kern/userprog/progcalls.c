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
#include <synch.h>
#include <pid.h>

//TODO: need to find a good place to destroy this guy
struct lock *mutex;

void
sys_init() {
	mutex = lock_create("sys_mutex");
}

int
sys__exit(int exitcode)
{
	lock_acquire(mutex);
	
	pid_setexitcode(curthread->t_pid, exitcode);
	cv_broadcast(curthread->t_cvwaitpid, mutex);

	lock_release(mutex);

	thread_exit();

	return 0;
}

int
sys_getpid(int *retval)
{
	*retval = curthread->t_pid;
	return 0;
}

int
sys_waitpid(pid_t pid, userptr_t status, int options, int *retval)
{
	lock_acquire(mutex);

	if(options != 0) {
		lock_release(mutex);
		return EINVAL;
	}

	int exitcode = pid_getexitcode(pid);
	if(exitcode != -1) {
		copyout(&exitcode, status, sizeof(int));
		*retval = (int)pid;
		lock_release(mutex);
		return 0;
	}
	struct thread *thread = pid_getthread(pid);

	if(thread == NULL) {
		lock_release(mutex);
		return EINVAL;
	}
	if(pid_is_child(curthread, pid)) {
		lock_release(mutex);
		return EINVAL;
	}

	cv_wait(thread->t_cvwaitpid, mutex);

	exitcode = pid_getexitcode(pid);

	copyout(&exitcode, status, sizeof(int));
	*retval = (int)pid;

	lock_release(mutex);

	return 0;
}













