/*
	pid.h

*/
#ifndef _PID_H_
#define _PID_H_

#define PID_MAX 1024 /* Maximum number of concurrently running processes */

void pid_setuptable(void);
pid_t pid_getnext(struct thread *master);
struct thread * pid_getthread(pid_t pid);
int pid_getexitcode(pid_t pid);
void pid_setexitcode(pid_t pid, int exitcode);
void pid_clear(pid_t pid);
int pid_is_child(struct thread *thread, pid_t pid2);

#endif /* _PID_H_ */
