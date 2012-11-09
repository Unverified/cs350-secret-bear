/*
	pid.h

*/
#ifndef _PID_H_
#define _PID_H_

void pid_setuptable(void);
void pid_destroy(void);

pid_t pid_getnext(struct thread *master);
struct thread * pid_getthread(pid_t pid);
int pid_getexitcode(pid_t pid);
void pid_setexitcode(pid_t pid, int exitcode);
void pid_clear(pid_t pid);
int pid_is_child(struct thread *thread, pid_t pid2);


#endif /* _PID_H_ */
