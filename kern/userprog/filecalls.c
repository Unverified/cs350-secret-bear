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
	
	if(curthread->t_filetable[fd] == NULL){
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
sys_read(int fd, userptr_t data, size_t size, int *retval)
{
	if(fd_check_valid(fd)){
		return EBADF;
	}

	char buffer[size]; //have a buffer in kern space to read to
	struct fd *des  = curthread->t_filetable[fd];	
	
	struct uio uio;
	mk_kuio(&uio, buffer, size, des->offset, UIO_READ);
	
	int result = VOP_READ(des->vnode, &uio);
	if(result){
		return result;
	}
	
	result = copyout(buffer, data, size);
	if(result){
		return result;	
	}
	
	des->offset = uio.uio_offset;
	*retval = size - (int)uio.uio_resid;
	return 0;
}

int
sys_write(int fd, const_userptr_t data, size_t size, int *retval)
{
	if(size == 0)
		return 0;

	if(fd_check_valid(fd)){
		return EBADF;
	}
	
	//I was getting errors and junk with the prev way so I make this was, it seems to be working for
	//me. With the prev stuff if I ran "testbin/badtest a" i would get :
	//	panic: Assertion failed: (vaddr_t)ptr%PAGE_SIZE==0, at ../../lib/kheap.c:574 (kfree)
	char *k_data = kmalloc(sizeof(char)*(size));
	size_t actual;
	int result = copyin(data, k_data, size);
	if(result){
		kprintf("failed\n");
		return result;
	}

	//set up a spot in kernel space to read out
	//char k_data[size]; // all chars a 1 byte so cheat the system
	//int result = copyin(data, (void*)k_data, size);
	//if(result){
	//	return result;
	//}
	
	struct uio uio;
	struct fd *des = curthread->t_filetable[fd];
	mk_kuio(&uio, k_data, size, des->offset, UIO_WRITE);

	//write to the device, record if there is an error
	result = VOP_WRITE(des->vnode, &uio);
	if(result){
		kfree(k_data);
		return result;
	}
	
	//update offset and return value, success!
	des->offset = uio.uio_offset;
	*retval = (int)uio.uio_resid;

	kfree(k_data);
	
	return 0;
}

struct fd*
fd_init(char *name, struct vnode *node, int flag)
{
	struct fd* new_fd = kmalloc(sizeof(struct fd));
	
	new_fd->filename = name;
	new_fd->vnode = node;
	new_fd->offset = 0;
	new_fd->flags = flag;
	
	return new_fd;
}

void
fd_init_initial(struct thread* t)
{
	// setup stdin
	struct vnode *stin; 
	char *stin_n = kstrdup("con:");
	vfs_open(stin_n, O_RDONLY, &stin);
	t->t_filetable[0] = fd_init(stin_n, stin, O_RDONLY);

	// setup stout
	struct vnode *stout;
	char * stout_n = kstrdup("con:");
	vfs_open(stout_n, O_WRONLY, &stout);
	t->t_filetable[1] = fd_init(stout_n, stout, O_WRONLY);

	// setup sterr
	struct vnode *sterr;
	char *sterr_n = kstrdup("con:");
	vfs_open(sterr_n, O_WRONLY, &sterr);
	t->t_filetable[2] = fd_init(sterr_n, sterr, O_WRONLY);
}

struct fd*
fd_copy(struct fd *master)
{
	struct fd* copy = kmalloc(sizeof(struct fd));
	if(copy == NULL){
		return NULL;
	}
	
	char * name = kstrdup(master->filename);
	if(name == NULL){
		return NULL;
	}
	
	struct vnode *copy_node = kmalloc(sizeof(struct vnode));
	if(copy_node == NULL){
		return NULL;
	}
	vfs_open(name, master->flags, &copy_node);
	
	copy->filename = name;
	copy->vnode = copy_node;
	copy->offset = master->offset;
	copy->flags = master->flags;
	
	return copy;
}

void
fd_destroy(struct fd *des)
{
	kfree(des->filename);
	
	vfs_close(des->vnode);
	
	kfree(des);
}

/*
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

*/
