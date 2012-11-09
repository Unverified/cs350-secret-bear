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

static struct lock * fd_mutex;

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
	int fd = -1;
	int result, i;
	char *k_fname;
	struct vnode * v_open;

	// find next free fd...
	for (i = 3; i <= MAX_FD; i++)
	{
		if (curthread->t_filetable[i] == NULL) {
			fd = i;
			break;
		}
	}
	if (fd == -1) {
		return EMFILE;	// fd table full!
	}

	k_fname = kmalloc(sizeof(char)*NAME_MAX);
	if(k_fname == NULL){
		return ENOMEM;
	}
	size_t size;
	result = copyinstr(filename, k_fname, sizeof(char)*NAME_MAX, &size);
	if(result){
		goto open_err;
	}
	result = vfs_open(k_fname, flags, &v_open);		
	if (result) {	
		goto open_err;		
	}
	result = fd_init(k_fname, v_open, flags, &(curthread->t_filetable[fd]));
	if(result){
		vfs_close(v_open);
		goto open_err;
	}
		
		*retval = fd;
	
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

	if(data == NULL){
		return EFAULT;
	}

	char *k_data = kmalloc(sizeof(char)*size);
	if(k_data == NULL){
		return ENOMEM;
	}
	
	struct uio uio;
	struct fd *des  = curthread->t_filetable[fd];	
	if(des->flags != O_RDWR && des->flags != O_RDONLY){
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

	//kfree(k_data);	
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
	if(des->flags != O_RDWR && des->flags != O_WRONLY){
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
	*retval = (int)uio.uio_resid;
	
	return 0;
}

int
fd_init(char *name, struct vnode *node, int flag, struct fd **retval)
{
	struct fd* new_fd = kmalloc(sizeof(struct fd));
	if(new_fd == NULL){
		return ENOMEM;
	}
	
	new_fd->filename = name;
	new_fd->vnode = node;
	new_fd->offset = 0;
	new_fd->flags = flag;
		
	*retval = new_fd;
	return 0;
}

int
fd_init_initial(struct thread* t)
{
	int ret;
	
	// setup stdin
	struct vnode *stin; 
	char *stin_n = kstrdup("con:");
	if(stin_n == NULL){
		return ENOMEM;
	}
	ret = vfs_open(stin_n, O_RDONLY, &stin);
	if(ret){
		kfree(stin_n);
		return ret;
	}
	ret = fd_init(stin_n, stin, O_RDONLY, &(t->t_filetable[0]));
	if(ret){
		kfree(stin_n);
		vfs_close(stin);
		return ret;
	}

	// setup stout
	struct vnode *stout;
	char *stout_n = kstrdup("con:");
	if(stout_n == NULL){
		fd_destroy(t->t_filetable[0]);
		t->t_filetable[0] = NULL;
		return ENOMEM;
	}
	ret = vfs_open(stout_n, O_WRONLY, &stout);
	if(ret){
		kfree(stout_n);
		fd_destroy(t->t_filetable[0]);
		t->t_filetable[0] = NULL;
		return ret;
	}
	ret = fd_init(stout_n, stout, O_WRONLY, &(t->t_filetable[1]));
	if(ret){
		kfree(stout_n);
		vfs_close(stout);
		fd_destroy(t->t_filetable[0]);
		t->t_filetable[0] = NULL;
		return ret;
	}
	
	// setup sterr
	struct vnode *sterr;
	char *sterr_n = kstrdup("con:");
	if(sterr_n == NULL){
		fd_destroy(t->t_filetable[0]);
		fd_destroy(t->t_filetable[1]);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ENOMEM;
	}
	ret = vfs_open(sterr_n, O_WRONLY, &sterr);
	if(ret){
		kfree(sterr_n);
		fd_destroy(t->t_filetable[0]);
		fd_destroy(t->t_filetable[1]);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ret;
	}
	ret = fd_init(sterr_n, sterr, O_WRONLY, &(t->t_filetable[2]));
	if(ret){
		kfree(sterr_n);
		vfs_close(sterr);
		fd_destroy(t->t_filetable[0]);
		fd_destroy(t->t_filetable[1]);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ret;
	}

	return 0;
}

int
fd_copy(struct fd *master, struct fd **retval)
{
	int ret;
	struct fd *copy;

	char *name = kstrdup(master->filename);
	if(name == NULL){
		return ENOMEM;
	}
	
	struct vnode *copy_node;
	ret = vfs_open(name, master->flags, &copy_node);
	if(ret){
		kfree(name);
		return ret;
	}
	
	ret = fd_init(name, copy_node, master->flags, &copy);
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
	kfree(des->filename);
	vfs_close(des->vnode);
	
	kfree(des);
}
