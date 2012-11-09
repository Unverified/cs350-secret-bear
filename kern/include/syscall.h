#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include "opt-A2.h"
#include <machine/trapframe.h>
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

#if OPT_A2
/* SYS__exit system call
 * code resides in /kern/userprog/progcalls.c
 */
int sys__exit(int exitcode); 

/* SYS_execv system call
 * code resides in /kern/userprog/execv.c
 */
int sys_execv(char *progname, char **args);

/* SYS_fork system call
 * code resides in /kern/thread/thread.c
 */
int sys_fork(struct trapframe *tf, int *retval); 

/* SYS_waitpid system call
 * code resides in /kern/userprog/progcalls.c
 */
int sys_waitpid(pid_t pid, userptr_t status, int options, int *retval);

/* SYS_open system call
 * code resides in /kern/userprog/filecalls.c
 */
int sys_open(const char * filename, int flags, int *retval);

/* SYS_close system call
 * code resides in /kern/userprog/filecalls.c
 */
int sys_close(int fd, int *retval);

/* SYS_read system call
 * code resides in /kern/userprog/filecalls.c
 */
int sys_read(int fd, userptr_t data, size_t len, int *retval);

/* SYS_write system call
 * code resides in /kern/userprog/filecalls.c
 */
int sys_write(int fd, const_userptr_t data, size_t len, int *retval);

/* SYS_getpid system call
 * code resides in /kern/userprog/progcalls.c
 */
int sys_getpid(int *retval);
#endif /* OPT_A2 */
#endif /* _SYSCALL_H_ */
