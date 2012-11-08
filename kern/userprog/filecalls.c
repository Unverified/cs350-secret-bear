/*
filecalls.c

Implementation of all file related system calls:
	- open
	- close
	- read
	- write

*/

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <syscall.h>
#include <filecalls.h>
#include <curthread.h>
#include <thread.h>
#include <kern/unistd.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>

static
int
fd_check_valid(int fd)
{
	if(fd < 0 || fd > MAX_FD){
		return 1;
	}
	
	if(curthread->fdt->table[fd].vnode == NULL){
		return 1;
	}
	
	return 0;
}

int
sys_open(int *retval)
{
	kprintf("open called!\n");
	
	(void) retval;
	return 0;
}

int
sys_close(int fd, int *retval)
{
	if(fd_check_valid(fd)){
		return EBADF;
	}
	kprintf("close called!\n");
	
	(void) retval;
	return 0;
}

int
sys_read(int fd, int *retval)
{
	if(fd_check_valid(fd)){
		return EBADF;
	}
	kprintf("read called!\n");
	
	(void) retval;
	return 0;
}

int
sys_write(int fd, const_userptr_t data, size_t size, int *retval)
{
	if(fd_check_valid(fd)){
		return EBADF;
	}
	
	struct vnode *v_write = curthread->fdt->table[fd].vnode;
	off_t offset = curthread->fdt->table[fd].offset;
	
	//set up a spot in kernel space to read out
	char k_data[size]; // all chars a 1 byte so cheat the system
	copyin(data, (void*)k_data, size);
	
	struct uio uio;
	mk_kuio(&uio, k_data, size, offset, UIO_WRITE);

	//write to the device, record if there is an error
	int result = VOP_WRITE(v_write, &uio);
	if(result){
		return result;
	}
	
	//update offset and return value, success!
	curthread->fdt->table[fd].offset = uio.uio_offset;
	*retval = (int)uio.uio_resid;
	
	return 0;
}

struct fdt* 
fdt_init ()
{
	struct fdt *newtable;
	newtable = kmalloc(sizeof (struct fdt));

	// set vnodes to NULL
	int i;
	for (i = 0; i <= MAX_FD; i++)
	{
		newtable->table[i].vnode = NULL;	
	}

	// setup stdin, stdout, stderr
	struct vnode * stin;
	char * first = NULL;
	first = kstrdup("con:");
	vfs_open(first, O_RDONLY, &stin);
	newtable->table[0].filename = first;
	newtable->table[0].vnode = stin;
	newtable->table[0].offset = 0;
	newtable->table[0].flags = O_RDONLY;

	struct vnode * stout;
	char * second = NULL;
	second = kstrdup("con:");
	vfs_open(second, O_WRONLY, &stout);
	newtable->table[1].filename = second; 
	newtable->table[1].vnode = stout;
	newtable->table[1].offset = 0;
	newtable->table[1].flags = O_WRONLY;

	struct vnode * sterr;
	char * third = NULL;
	third = kstrdup("con:");
	vfs_open(third, O_WRONLY, &sterr);
	newtable->table[2].filename = third;
	newtable->table[2].vnode = sterr;
	newtable->table[2].offset = 0;
	newtable->table[2].flags = O_WRONLY;	

	return newtable;
}

void
fdt_free(struct fdt * oldtable)
{
	kfree(oldtable);
}

int fdt_add (struct fdt * fdt, const char * filename, struct vnode * vnode, int flags)
{
        int fdNum = -1;

        // find next available fd entry
        int i;
        for (i = 0; i <= fdt->max; i++)
        {
                if (fdt->table[i].vnode == NULL)
                {
                        fdNum = i;
                        break;
                }
        }
	
	// add new fd
        if (fdNum != -1)
        {
		char * name;
		name = kstrdup(filename);
                fdt->table[i].filename = name;
                fdt->table[i].vnode = vnode;
                fdt->table[i].offset = 0;
                fdt->table[i].flags = flags;
        }

        return fdNum;
}

