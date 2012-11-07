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


#define MAX_FD 10		// maximum size of File Descriptor Table
#define MAX_FILENAME_LEN 15	// maximum size of file name 

struct fd {
	char fileName[MAX_FILENAME_LEN];
	struct vnode * vnode;	// See kern/include/vnode.h for more info
	off_t offset;			// File pointer offset
	size_t flags;			// read, write or append permissions
};

#endif /* _FILE_CALLS_H_ */
