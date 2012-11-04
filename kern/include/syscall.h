#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include "opt-A2.h"
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

#if OPT_A2
/* SYS_open system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_open(int *retval);

/* SYS_close system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_close(int *retval);

/* SYS_read system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_read(int *retval);

/* SYS_write system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_write(int fd, const_userptr_t data, size_t len, int *retval);

/* SYS__exit system call
 * code resides in /kern/userspace/progcalls.c
 */
int sys__exit(int *retval); 

/* SYS_fork system call
 * code resides in /kern/userspace/progcalls.c
 */
int sys_fork(int *retval); 

#endif /* OPT_A2 */
#endif /* _SYSCALL_H_ */
