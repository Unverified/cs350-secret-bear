#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <synch.h>
#include <pid.h>

static struct thread *pid_table[PID_MAX];

void
pid_setuptable()
{
	int i;
	
	if(pid_table == NULL){
		panic("unable to allocate pid table");
	}
	 
	for (i = 0; i < PID_MAX; i++){
		pid_table[i] = NULL;
	}
}

pid_t
pid_getnext(struct thread *master)
{
	assert(master != NULL);
	pid_t i = 0;
	
	for(i = 0; pid_table[i] != NULL || i == PID_MAX; i++){}
	 
	if(i < PID_MAX){
		pid_table[i] = master;
		return i + 1;
	}
	//table is full, not sure what to do here
	return 0;
}

struct thread*
pid_getthread(pid_t pid)
{
	struct thread *retval;
	retval = pid_table[pid - 1];
	
	return retval;
}

void
pid_clear(pid_t pid)
{
	assert(pid > 0);
	assert(pid <= PID_MAX);
	
	pid_table[pid - 1] = NULL;
}
