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
int sys_open();


/* SYS_close system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_close();


/* SYS_read system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_read();


/* SYS_write system call
 * code resides in /kern/userspace/filecalls.c
 */
int sys_write(int fd, const_userptr_t data, int len, int *retval);

#endif /* OPT_A2 */
#endif /* _SYSCALL_H_ */
