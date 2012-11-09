#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <curthread.h>
#include <synch.h>
#include <pid.h>

static struct lock *pid_mutex;
static struct thread *pid_table[PID_MAX];
static int exitcodes[PID_MAX];

void
pid_setuptable()
{
	int i;
	
	if(pid_table == NULL){
		panic("unable to allocate pid table");
	}
	 
	pid_mutex = lock_create("pid_mutex");
	if(pid_mutex == NULL){
		panic("unable to create pid lock");
	}
	
	for (i = 0; i < PID_MAX; i++){
		pid_table[i] = NULL;
		exitcodes[i] = -1;
	}
}

void
pid_destroy()
{
	lock_destroy(pid_mutex);
}

pid_t
pid_getnext(struct thread *master)
{
	assert(master != NULL);
	
	int i, retval = 0;
	int search = random() % PID_MAX;
	
	lock_acquire(pid_mutex);
	for(i = 1; pid_table[search] != NULL && i < PID_MAX; i++){
		search++;
		if(search == PID_MAX){
			search = 1;
		}
	}
	 
	if(i < PID_MAX){
		pid_table[search] = master;
		exitcodes[search] = -1;
		retval = search + 1;
	}
	lock_release(pid_mutex);
	
	return retval;
}

struct thread*
pid_getthread(pid_t pid)
{
	struct thread *retval;
	
	lock_acquire(pid_mutex);
	retval = pid_table[pid - 1];
	lock_release(pid_mutex);
	
	return retval;
}

int
pid_getexitcode(pid_t pid)
{
	int retval;
	retval = exitcodes[pid - 1];
	
	return retval;
}

void
pid_setexitcode(pid_t pid, int exitcode)
{
	exitcodes[pid - 1] = exitcode;
}

int pid_is_child(struct thread *thread, pid_t pid2) {
	while(thread->t_ppid != 0) {
		if(thread->t_ppid == pid2)
			return 1;

		thread = pid_getthread(thread->t_ppid);
	}

	return 0;
}

void
pid_clear(pid_t pid)
{
	if (pid <= 0 || pid > PID_MAX) return;
	
	lock_acquire(pid_mutex);
	pid_table[pid - 1] = NULL;
	lock_release(pid_mutex);
}
