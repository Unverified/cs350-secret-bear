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
sys_open(const char * filename, int flags, int *retval)
{
	int fd = -1;
	int result, i;
	char * name;
	struct vnode * v_open;


	// find next free fd...
	for (i = 0; i <= MAX_FD; i++)
	{
		if (curthread->t_filetable[i] == NULL) {
			fd = i;
			break;
		}
	}

	if (fd == -1) {
		*retval = EMFILE;	// fd table full!
	}
	else {		// actually open file
		name = kstrdup(filename);
		result = vfs_open(name, flags, &v_open);
		
		if (result) {		// vfs_open fail
			return result;		
		}
	
		curthread->t_filetable[fd] = fd_init(name, v_open, flags);	
		*retval = result;
	}	
	
	return 0;
}

int
sys_close(int fd, int *retval)
{
	if(fd_check_valid(fd) || curthread->t_filetable[fd] == NULL){
		 *retval = EBADF;
	}

	vfs_close (curthread->t_filetable[fd]->vnode);

	fd_destroy (curthread->t_filetable[fd]);

	*retval = 0;

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
	if(fd_check_valid(fd)){
		return EBADF;
	}
	
	char k_data[size];

	//set up a spot in kernel space to read out
	int result = copyin(data, (void*)k_data, size);
	if(result){
		return result;
	}
	
	struct uio uio;
	struct fd *des = curthread->t_filetable[fd];
	mk_kuio(&uio, k_data, size, des->offset, UIO_WRITE);

	//write to the device, record if there is an error
	result = VOP_WRITE(des->vnode, &uio);
	if(result){
		return result;
	}
	
	//update offset and return value, success!
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
	
	struct fd *fd_stin;
	ret = fd_init(stin_n, stin, O_RDONLY, &fd_stin);
	if(ret){
		kfree(stin_n);
		vfs_close(stin);
		return ret;
	}
	t->t_filetable[0] = fd_stin;

	// setup stout
	struct vnode *stout;
	char *stout_n = kstrdup("con:");
	if(stout_n == NULL){
		fd_destroy(fd_stin);
		t->t_filetable[0] = NULL;
		return ENOMEM;
	}
	
	ret = vfs_open(stout_n, O_WRONLY, &stout);
	if(ret){
		kfree(stout_n);
		fd_destroy(fd_stin);
		t->t_filetable[0] = NULL;
		return ret;
	}
	struct fd *fd_stout;
	ret = fd_init(stout_n, stout, O_WRONLY, &fd_stout);
	if(ret){
		kfree(stout_n);
		vfs_close(stout);
		fd_destroy(fd_stin);
		t->t_filetable[0] = NULL;
		return ret;
	}
	t->t_filetable[1] = fd_stout;

	// setup sterr
	struct vnode *sterr;
	char *sterr_n = kstrdup("con:");
	if(sterr_n == NULL){
		fd_destroy(fd_stin);
		fd_destroy(fd_stout);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ENOMEM;
	}
	ret = vfs_open(sterr_n, O_WRONLY, &sterr);
	if(ret){
		kfree(sterr_n);
		fd_destroy(fd_stin);
		fd_destroy(fd_stout);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ret;
	}
	struct fd *fd_sterr;
	ret = fd_init(sterr_n, sterr, O_WRONLY, &fd_sterr);
	if(ret){
		kfree(sterr_n);
		vfs_close(sterr);
		fd_destroy(fd_stin);
		fd_destroy(fd_stout);
		t->t_filetable[0] = NULL;
		t->t_filetable[1] = NULL;
		return ret;
	}
	t->t_filetable[2] = fd_sterr;
	
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
	//vfs_close(des->vnode);
	
	kfree(des);
}
