#ifndef _KERN_LIMITS_H_
#define _KERN_LIMITS_H_

/* Longest filename (without directory) not including null terminator */
#define NAME_MAX  255

/* Longest full path name */
#define PATH_MAX   1024

/* Largest amount of Process ID's that can be used at a time */
#define PID_MAX 128

/* The amount of file descriptors avalible per thread */
#define MAX_FD 10

#endif /* _KERN_LIMITS_H_ */
