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
#include <kern/limits.h>
#include <synch.h>

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
sys_open(const_userptr_t filename, int flags, int *retval)
{
	size_t size;
	int result, i;
	char *k_fname;

	// bad flag
	if (flags >= 64 || flags < 0) {  
		return EINVAL;
	}

	// bad filename pointer
	if (filename == NULL) {
		return EFAULT;
	}

	// find next free fd, 0-2 is saved...
	for (i = 3; i < MAX_FD; i++)
	{
		if (curthread->t_filetable[i] == NULL) {
			break;
		}
	}
	if (i == MAX_FD) {
		return EMFILE;	// fd table full!
	}

	k_fname = kmalloc(sizeof(char)*NAME_MAX);
	if(k_fname == NULL){
		return ENOMEM;
	}
	
	result = copyinstr(filename, k_fname, sizeof(char)*NAME_MAX, &size);
	if(result){
		goto open_err;
	}

	result = fd_init(k_fname, flags, &(curthread->t_filetable[i]));
	if(result){
		goto open_err;
	}
		
	*retval = i;
	return 0;
	
  open_err:
	kfree(k_fname);
	return result;
}

int
sys_close(int fd, int *retval)
{
	if(fd_check_valid(fd) || curthread->t_filetable[fd] == NULL){
		*retval = -1;
		return EBADF;
	}

	fd_destroy(curthread->t_filetable[fd]);
	curthread->t_filetable[fd] = NULL;

	return 0;
}

int
sys_read(int fd, userptr_t data, size_t size, int *retval)
{
	if(size == 0){
		return 0;
	}
	if(fd_check_valid(fd)){
		return EBADF;
	}

	if(data == NULL || (void*)data >= (void*)0x80000000){
		return EFAULT;
	}

	char *k_data = kmalloc(sizeof(char)*size);
	if(k_data == NULL){
		return ENOMEM;
	}
	
	struct uio uio;
	struct fd *des  = curthread->t_filetable[fd];	
	
	// check flags for read permissions
	// using bitwise checks
	if (!(des->flags & O_RDWR) && (des->flags & O_WRONLY)) {
		return EINVAL;
	}

	mk_kuio(&uio, k_data, size, des->offset, UIO_READ);

	int result = VOP_READ(des->vnode, &uio);
	
	if(result){
		kfree(k_data);
		return result;
	}
	
	result = copyout(k_data, data, size);
	if(result){
		kfree(k_data);
		return result;	
	}

	kfree(k_data);	
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

	// data buffer invalid
	if ((data == NULL) || 
		((void*)data >= (void*)0x80000000) ||
		((void*)data >= (void*)0x00400000 && (void*)data <= (void*)0x00401a0c)) {	
		return EFAULT;
	}	

	char *k_data = kmalloc(sizeof(char)*(size));
	if(k_data == NULL){
		return ENOMEM;
	}
	
	int result = copyin(data, k_data, size);
	if(result){
		kfree(k_data);
		return result;
	}
	
	struct uio uio;
	struct fd *des = curthread->t_filetable[fd];	
	
	// check flags for write permissions
	// using bitwise checks
	if (!(des->flags & O_RDWR) && !(des->flags & O_WRONLY)) {
		return EINVAL;
	}

	mk_kuio(&uio, k_data, size, des->offset, UIO_WRITE);

	//write to the device, record if there is an error
	result = VOP_WRITE(des->vnode, &uio);
	if(result){
		kfree(k_data);
		return result;
	}
	
	//update offset and return value, success!
	kfree(k_data);
	des->offset = uio.uio_offset;
	*retval = size - (int)uio.uio_resid;
	
	return 0;
}

static int initcount = 0;
static int destcount = 0;

int
fd_init(char *name, int flag, struct fd **retval)
{
	
	int ret;
	struct fd* new_fd = kmalloc(sizeof(struct fd));
	
	if(new_fd == NULL){
		return ENOMEM;
	}
	
	struct vnode *vnode;
	ret = vfs_open(name, flag, &vnode);
	if(ret){
		return ret;
	}
	
	initcount++;
	
	new_fd->filename = name;
	new_fd->vnode = vnode;
	new_fd->offset = 0;
	new_fd->flags = flag;
		
	*retval = new_fd;
	return 0;
}

int
fd_init_initial(struct thread* t)
{
	int ret;
	
	// setup standard input
	char *stdin = kstrdup("con:");
	if(stdin == NULL){
		return ENOMEM;
	}

	ret = fd_init(stdin, O_RDONLY, &(t->t_filetable[0]));
	if(ret){
		kfree(stdin);
		return ret;
	}

	// setup standard out
	char *stdout = kstrdup("con:");
	if(stdout == NULL){
		ret = ENOMEM;
		goto fail0;
	}
	
	ret = fd_init(stdout, O_WRONLY, &(t->t_filetable[1]));
	if(ret){
		kfree(stdout);
		goto fail0;
	}
	
	// setup standard error
	char *stderr = kstrdup("con:");
	if(stderr == NULL){
		ret = ENOMEM;
		goto fail1;
	}

	ret = fd_init(stderr, O_WRONLY, &(t->t_filetable[2]));
	if(ret){
		kfree(stderr);
		goto fail1;
	}

	return 0;
	
  fail1:
	fd_destroy(t->t_filetable[1]);
	t->t_filetable[1] = NULL;
	
  fail0:
	fd_destroy(t->t_filetable[0]);
	t->t_filetable[0] = NULL;
	return ret;
}

int
fd_copy(struct fd *master, struct fd **retval)
{
	int ret;
	struct fd *copy;
	
	if(master == NULL){
		*retval = NULL;
		return 0;
	}

	char *name = kstrdup(master->filename);
	if(name == NULL){
		return ENOMEM;
	}
	
	ret = fd_init(name, master->flags, &copy);
	if(ret) {
		kfree(name);
		return ret;
	}
	copy->offset = master->offset;
	
	*retval = copy;
	return 0;
}

void
fd_destroy(struct fd *des)
{
	if(des == NULL){ return; }
	
	destcount++;
	kprintf("inc count: %d dest count %d\n", initcount, destcount);

	if(des->filename != NULL){
		kfree(des->filename);
		des->filename = NULL;
	}
	
	if(des->vnode != NULL){
		vfs_close(des->vnode);
		des->vnode = NULL;
	}
	
	kfree(des);
}
