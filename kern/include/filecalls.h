/* 

filecalls.h

Implementation of FILE DESCRIPTORS

Each thread has its own array of fd's, keeping track of all open files

Note the special entries in the table:
	- 0 - stdin
	- 1 - stdout
	- 2 - stderr

*/
#ifndef _FILE_CALLS_H_
#define _FILE_CALLS_H_

#include <types.h>
#include <vnode.h>
#include <uio.h>
#include <array.h>
#include <kern/limits.h>

//cyclic deps are fun
struct thread;

struct fd{
	char * filename;
	struct vnode * vnode;	// See kern/include/vnode.h for more info
	off_t offset;			// File pointer offset
	size_t flags;			// read, write or append permissions
};

// Initialize fdt 
// Sets all vnodes to NULL
// Initializes stdin, stdout, stderr
// struct fdt * fdt_init (void);

// free fdt
// void fdt_free(struct fdt * oldtable);

// Add entry to File Descript Table, returns the fd number
// Returns -1 on error
// int fdt_add (struct fdt * fdt, const char * filename, struct vnode * vnode, int flags);

struct fd * fd_init(char *name, struct vnode *node, int flag);

void fd_init_initial(struct thread * t);

struct fd * fd_copy(struct fd *master);

void fd_destroy(struct fd *des);

#endif /* _FILE_CALLS_H_ */
