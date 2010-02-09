#define _XOPEN_SOURCE 600

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <utime.h>

#include <avl.h>

#include "rfsd.h"

static bool fd_key_set = false;
static uint64_t fd_key = 0;

struct file_node {
	struct avl_node avl;
	uint64_t key;
	int fd;
};
static struct avl files;

struct dir_node {
	struct avl_node avl;
	uint64_t key;
	DIR *dir;
};
static struct avl dirs;

static int file_node_cmp(const struct file_node *x, const struct file_node *y)
{
	if (x->key < y->key) {
		return -1;
	} else if (x->key == y->key) {
		return 0;
	} else {
		return 1;
	}
}

static void file_node_free(struct file_node *p)
{
	close(p->fd);
	free(p);
}

static int dir_node_cmp(const struct dir_node *x, const struct dir_node *y)
{
	if (x->key < y->key) {
		return -1;
	} else if (x->key == y->key) {
		return 0;
	} else {
		return 1;
	}
}

static void dir_node_free(struct dir_node *p)
{
	closedir(p->dir);
	free(p);
}

void rfs_init(void)
{
	avl_init(&files, offsetof(struct file_node, avl),
		(avl_cmp_t)file_node_cmp);
	avl_init(&dirs, offsetof(struct dir_node, avl),
		(avl_cmp_t)dir_node_cmp);

	umask(0);
}

void rfs_destroy(void)
{
	avl_traverse(&files, (avl_process_t)file_node_free);
	avl_traverse(&dirs, (avl_process_t)dir_node_free);
}

static void stat2x_stat(x_stat *dst, const struct stat *src)
{
	dst->mode = src->st_mode;
	dst->nlink = src->st_nlink;
	dst->uid = src->st_uid;
	dst->gid = src->st_gid;
	dst->rdev = src->st_rdev;
	dst->size = src->st_size;
	dst->atime = src->st_atime;
	dst->mtime = src->st_mtime;
	dst->ctime = src->st_ctime;
}

int32_t r_set_key(struct ipc *ipc, const uint64_t *key)
{
	(void)ipc;

	if (fd_key)
		return EEXIST;

	fd_key = *key;
	fd_key_set = true;
	return 0;
}

int32_t r_getattr(struct ipc *ipc, const string *path, x_stat *buf)
{
	(void)ipc;

	struct stat st;
	if (lstat(path->cs, &st) == -1)
		return errno;

	stat2x_stat(buf, &st);
	return 0;
}

int32_t r_readlink(struct ipc *ipc, const string *path, const uint32_t *len,
		string *buf)
{
	buf->s = mpool_alloc(&ipc->mp, *len);
	if (buf->s == NULL)
		return ENOMEM;

	ssize_t n = readlink(path->cs, buf->s, *len - 1);
	if (n == -1)
		return errno;

	buf->s[n] = '\0';
	return 0;
}

int32_t r_mknod(struct ipc *ipc, const string *path, const x_mode *mode,
		const x_dev *dev)
{
	(void)ipc;

	return mknod(path->cs, *mode, *dev) == -1 ? errno : 0;
}

int32_t r_mkdir(struct ipc *ipc, const string *path, const x_mode *mode)
{
	(void)ipc;

	return mkdir(path->cs, *mode) == -1 ? errno : 0;
}

int32_t r_unlink(struct ipc *ipc, const string *path)
{
	(void)ipc;

	return unlink(path->cs) == -1 ? errno : 0;
}

int32_t r_rmdir(struct ipc *ipc, const string *path)
{
	(void)ipc;

	return rmdir(path->cs) == -1 ? errno : 0;
}

int32_t r_symlink(struct ipc *ipc, const string *oldpath,
		const string *newpath)
{
	(void)ipc;

	return symlink(oldpath->cs, newpath->cs) == -1 ? errno : 0;
}

int32_t r_rename(struct ipc *ipc, const string *oldpath, const string *newpath)
{
	(void)ipc;

	return rename(oldpath->cs, newpath->cs) == -1 ? errno : 0;
}

int32_t r_link(struct ipc *ipc, const string *oldpath, const string *newpath)
{
	(void)ipc;

	return link(oldpath->cs, newpath->cs) == -1 ? errno : 0;
}

int32_t r_chmod(struct ipc *ipc, const string *path, const x_mode *mode)
{
	(void)ipc;

	return chmod(path->cs, *mode) == -1 ? errno : 0;
}

int32_t r_chown(struct ipc *ipc, const string *path, const x_uid *owner,
		const x_gid *group)
{
	(void)ipc;

	return chown(path->cs, *owner, *group) == -1 ? errno : 0;
}

int32_t r_truncate(struct ipc *ipc, const string *path, const x_off *length)
{
	(void)ipc;

	return truncate(path->cs, *length) == -1 ? errno : 0;
}

int32_t r_open(struct ipc *ipc, const string *path, const int32_t *flags,
	const x_mode *mode, uint64_t *key)
{
	(void)ipc;

	struct file_node *p = malloc(sizeof(struct file_node));
	if (p == NULL)
		return ENOMEM;

	p->fd = open(path->cs, *flags, *mode);
	if (p->fd == -1) {
		free(p);
		return errno;
	}

	*key = p->key = fd_key++;
	avl_insert(&files, p);
	return 0;
}

int32_t r_read(struct ipc *ipc, const uint64_t *key, const uint32_t *size,
	const x_off *offset, datum *buf)
{
	struct file_node *p;
	p = avl_search(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	buf->p = mpool_alloc(&ipc->mp, *size);
	if (buf->p == NULL)
		return ENOMEM;

	int32_t n = pread(p->fd, buf->p, *size, *offset);
	if (n == -1)
		return errno;

	buf->n = n;
	return 0;
}

int32_t r_write(struct ipc *ipc, const uint64_t *key, const x_off *offset,
		const datum *data, uint32_t *done)
{
	(void)ipc;

	struct file_node *p;
	p = avl_search(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	ssize_t res = pwrite(p->fd, data->p, data->n, *offset);
	if (res == -1)
		return errno;

	*done = res;
	return 0;
}

int32_t r_statfs(struct ipc *ipc, const string *path, x_statfs *buf)
{
	(void)ipc;

	struct statvfs st;
	if (statvfs(path->cs, &st) == -1)
		return errno;

	buf->bsize = st.f_bsize;
	buf->blocks = st.f_blocks;
	buf->bfree = st.f_bfree;
	buf->bavail = st.f_bavail;
	buf->files = st.f_files;
	buf->ffree = st.f_ffree;
	buf->namemax = st.f_namemax;
	return 0;
}

int32_t r_release(struct ipc *ipc, const uint64_t *key)
{
	(void)ipc;

	struct file_node *p;
	p = avl_remove(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	int res = close(p->fd);
	free(p);
	return res == -1 ? errno : 0;
}

int32_t r_fsync(struct ipc *ipc, const uint64_t *key)
{
	(void)ipc;

	struct file_node *p;
	p = avl_remove(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	return fsync(p->fd) == -1 ? errno : 0;
}

int32_t r_fdatasync(struct ipc *ipc, const uint64_t *key)
{
	(void)ipc;

	struct file_node *p;
	p = avl_remove(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	return fdatasync(p->fd) == -1 ? errno : 0;
}

int32_t r_opendir(struct ipc *ipc, const string *path, uint64_t *key)
{
	(void)ipc;

	struct dir_node *p = malloc(sizeof(struct dir_node));
	if (p == NULL)
		return ENOMEM;

	p->dir = opendir(path->cs);
	if (p->dir == NULL) {
		free(p);
		return errno;
	}

	*key = p->key = fd_key++;
	avl_insert(&dirs, p);
	return 0;
}

int32_t r_readdir(struct ipc *ipc, const uint64_t *key, list_string *names)
{
	struct dir_node *p;
	p = avl_search(&dirs, &(struct dir_node){.key = *key});
	if (p == NULL)
		return EBADF;

	memset(names, 0, sizeof(list_string));
	rewinddir(p->dir);

	for (;;) {
		errno = 0;

		struct dirent *de = readdir(p->dir);
		if (de == NULL) {
			if (errno != 0)
				return errno;
			break;
		}

		string name;
		name.s = mpool_alloc(&ipc->mp, strlen(de->d_name) + 1);
		if (name.s == NULL)
			return ENOMEM;

		strcpy(name.s, de->d_name);
		if (!list_append_string(&ipc->mp, names, &name))
			return ENOMEM;
	}

	return 0;
}

int32_t r_releasedir(struct ipc *ipc, const uint64_t *key)
{
	(void)ipc;

	struct dir_node *p;
	p = avl_remove(&dirs, &(struct dir_node){.key = *key});
	if (p == NULL)
		return EBADF;

	int res = closedir(p->dir);
	free(p);
	return res == -1 ? errno : 0;
}

int32_t r_access(struct ipc *ipc, const string *path, const int32_t *mode)
{
	(void)ipc;

	return access(path->cs, *mode) == -1 ? errno : 0;
}

int32_t r_ftruncate(struct ipc *ipc, const uint64_t *key, const x_off *length)
{
	(void)ipc;

	struct file_node *p;
	p = avl_search(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	return ftruncate(p->fd, *length) == -1 ? errno : 0;
}

int32_t r_fgetattr(struct ipc *ipc, const uint64_t *key, x_stat *buf)
{
	(void)ipc;

	struct file_node *p;
	p = avl_search(&files, &(struct file_node){.key = *key});
	if (p == NULL)
		return EBADF;

	struct stat st;
	if (fstat(p->fd, &st) == -1)
		return errno;

	stat2x_stat(buf, &st);
	return 0;
}

int32_t r_utimens(struct ipc *ipc, const string *path,
		const x_timespec *atime, const x_timespec *mtime)
{
	(void)ipc;

	struct utimbuf buf;
	buf.actime = atime->sec;
	buf.modtime = mtime->sec;

	return utime(path->cs, &buf) == -1 ? errno : 0;
}
