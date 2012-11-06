
#ifndef _PID_H_
#define _PID_H_

#define PID_MAX 128 /* Maximum number of concurrently running processes */

void pid_setuptable(void);

pid_t pid_getnext(struct thread *master);

struct thread * pid_getthread(pid_t pid);

void pid_clear(pid_t pid);

#endif /* _PID_H_ */
