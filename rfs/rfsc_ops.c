#define _XOPEN_SOURCE 600

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <avl.h>

#include "rfsc.h"

static uint64_t generation;
static uint64_t last_key;

struct fd_node {
	struct avl_node avl;
	uint64_t key;
	uint64_t generation;
};

static struct avl fds;

static int fd_node_cmp(const struct fd_node *x, const struct fd_node *y)
{
	if (x->key < y->key)
		return -1;
	else if (x->key == y->key)
		return 0;
	else
		return 1;
}

#define CHECK_GENERATION(fi) do {					\
		struct fd_node *p;					\
		p = avl_search(&fds, &(struct fd_node){.key = (fi)->fh}); \
									\
		if (p == NULL)						\
			return -EBADF;					\
									\
		if (p->generation < generation)				\
			return -EIO;					\
	} while (false)

#define CALL(expr) do {						\
		int32_t res = (expr);				\
								\
		if (res == 0)					\
			break;					\
								\
		if (ipc.ok)					\
			return -res;				\
								\
		if (rfs_recover(last_key))			\
			++generation;				\
								\
		return -EIO;					\
	} while (false)

static void x_stat2stat(struct stat *dst, const x_stat *src)
{
	dst->st_mode = src->mode;
	dst->st_nlink = src->nlink;
	dst->st_uid = src->uid;
	dst->st_gid = src->gid;
	dst->st_rdev = src->rdev;
	dst->st_size = src->size;
	dst->st_atime = src->atime;
	dst->st_mtime = src->mtime;
	dst->st_ctime = src->ctime;
}

static int fs_getattr(const char *path, struct stat *buf)
{
	x_stat st;
	CALL(r_getattr(&ipc, &(string){.cs = path}, &st));
	x_stat2stat(buf, &st);
	return 0;
}

static int fs_readlink(const char *path, char *buf, size_t len)
{
	uint32_t len32 = len;
	string s;
	CALL(r_readlink(&ipc, &(string){.cs = path}, &len32, &s));

	strncpy(buf, s.s, len);
	mpool_cleanup(&ipc.mp);
	return 0;
}

static int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	x_mode x_mode = mode;
	x_dev x_dev = dev;
	CALL(r_mknod(&ipc, &(string){.cs = path}, &x_mode, &x_dev));
	return 0;
}

static int fs_mkdir(const char *path, mode_t mode)
{
	x_mode x_mode = mode;
	CALL(r_mkdir(&ipc, &(string){.cs = path}, &x_mode));
	return 0;
}

static int fs_unlink(const char *path)
{
	CALL(r_unlink(&ipc, &(string){.cs = path}));
	return 0;
}

static int fs_rmdir(const char *path)
{
	CALL(r_rmdir(&ipc, &(string){.cs = path}));
	return 0;
}

static int fs_symlink(const char *oldpath, const char *newpath)
{
	CALL(r_symlink(&ipc, &(string){.cs = oldpath},
			&(string){.cs = newpath}));
	return 0;
}

static int fs_rename(const char *oldpath, const char *newpath)
{
	CALL(r_rename(&ipc, &(string){.cs = oldpath},
			&(string){.cs = newpath}));
	return 0;
}

static int fs_link(const char *oldpath, const char *newpath)
{
	CALL(r_link(&ipc, &(string){.cs = oldpath}, &(string){.cs = newpath}));
	return 0;
}

static int fs_chmod(const char *path, mode_t mode)
{
	x_mode x_mode = mode;
	CALL(r_chmod(&ipc, &(string){.cs = path}, &x_mode));
	return 0;
}

static int fs_chown(const char *path, uid_t owner, gid_t group)
{
	x_uid x_owner = owner;
	x_gid x_group = group;
	CALL(r_chown(&ipc, &(string){.cs = path}, &x_owner, &x_group));
	return 0;
}

static int fs_truncate(const char *path, off_t length)
{
	x_off x_length = length;
	CALL(r_truncate(&ipc, &(string){.cs = path}, &x_length));
	return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	(void)path;

	CHECK_GENERATION(fi);

	uint32_t size32 = size;
	x_off x_offset = offset;
	datum data;
	CALL(r_read(&ipc, &fi->fh, &size32, &x_offset, &data));

	memcpy(buf, data.p, data.n);
	mpool_cleanup(&ipc.mp);
	return data.n;
}

static int fs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	(void)path;

	CHECK_GENERATION(fi);

	x_off x_offset = offset;
	uint32_t done;
	CALL(r_write(&ipc, &fi->fh, &x_offset,
			&(datum){size, (void *)buf}, &done));

	return done;
}

static int fs_statfs(const char *path, struct statvfs *buf)
{
	x_statfs st;
	CALL(r_statfs(&ipc, &(string){.cs = path}, &st));

	buf->f_bsize = st.bsize;
	buf->f_blocks = st.blocks;
	buf->f_bfree = st.bfree;
	buf->f_bavail = st.bavail;
	buf->f_files = st.files;
	buf->f_ffree = st.ffree;
	buf->f_namemax = st.namemax;

	return 0;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
	(void)path;

	struct fd_node *p;
	p = avl_remove(&fds, &(struct fd_node){.key = fi->fh});

	if (p == NULL)
		return -EBADF;

	if (p->generation == generation)
		CALL(r_release(&ipc, &fi->fh));

	return 0;
}

static int fs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	(void)path;

	CHECK_GENERATION(fi);

	if (datasync)
		CALL(r_fdatasync(&ipc, &fi->fh));
	else
		CALL(r_fsync(&ipc, &fi->fh));

	return 0;
}

static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
	CALL(r_opendir(&ipc, &(string){.cs = path}, &fi->fh));

	struct fd_node *p = malloc(sizeof(*p));

	if (p == NULL) {
		CALL(r_releasedir(&ipc, &fi->fh));
		return -ENOMEM;
	}

	last_key = p->key = fi->fh;
	p->generation = generation;
	avl_insert(&fds, p);

	return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t fill,
		off_t offset, struct fuse_file_info *fi)
{
	(void)path;
	(void)offset;

	CHECK_GENERATION(fi);

	list_string names;
	CALL(r_readdir(&ipc, &fi->fh, &names));

	for (const string *p = names.p; p < names.p + names.n; ++p)
		fill(buf, p->s, NULL, 0);

	mpool_cleanup(&ipc.mp);
	return 0;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
	(void)path;

	struct fd_node *p;
	p = avl_remove(&fds, &(struct fd_node){.key = fi->fh});

	if (p == NULL)
		return -EBADF;

	if (p->generation == generation)
		CALL(r_releasedir(&ipc, &fi->fh));

	return 0;
}

static void *fs_init(struct fuse_conn_info *conn)
{
	(void)conn;

	avl_init(&fds, offsetof(struct fd_node, avl), (avl_cmp_t)fd_node_cmp);
	return NULL;
}

static void fs_destroy(void *null)
{
	(void)null;

	avl_traverse(&fds, free);
	rfs_destroy();
}

static int fs_access(const char *path, int mode)
{
	CALL(r_access(&ipc, &(string){.cs = path}, &mode));
	return 0;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	x_mode x_mode = mode;
	int32_t x_flags = fi->flags;
	CALL(r_open(&ipc, &(string){.cs = path}, &x_flags, &x_mode, &fi->fh));

	struct fd_node *p = malloc(sizeof(*p));

	if (p == NULL) {
		CALL(r_release(&ipc, &fi->fh));
		return -ENOMEM;
	}

	last_key = p->key = fi->fh;
	p->generation = generation;
	avl_insert(&fds, p);

	return 0;
}

static int fs_ftruncate(const char *path, off_t length,
			struct fuse_file_info *fi)
{
	(void)path;

	CHECK_GENERATION(fi);

	x_off x_length = length;
	CALL(r_ftruncate(&ipc, &fi->fh, &x_length));

	return 0;
}

static int fs_fgetattr(const char *path, struct stat *buf,
		       struct fuse_file_info *fi)
{
	(void)path;

	CHECK_GENERATION(fi);

	x_stat st;
	CALL(r_fgetattr(&ipc, &fi->fh, &st));

	x_stat2stat(buf, &st);
	return 0;
}

static int fs_utimens(const char *path, const struct timespec tv[2])
{
	CALL(r_utimens(&ipc, &(string){.cs = path},
			&(x_timespec) {tv[0].tv_sec, tv[0].tv_nsec},
			&(x_timespec) {tv[1].tv_sec, tv[1].tv_nsec}));
	return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
	return fs_create(path, 0, fi);
}

const struct fuse_operations fs_ops = {
	.getattr = fs_getattr,
	.readlink = fs_readlink,
	.mknod = fs_mknod,
	.mkdir = fs_mkdir,
	.unlink = fs_unlink,
	.rmdir = fs_rmdir,
	.symlink = fs_symlink,
	.rename = fs_rename,
	.link = fs_link,
	.chmod = fs_chmod,
	.chown = fs_chown,
	.truncate = fs_truncate,
	.open = fs_open,
	.read = fs_read,
	.write = fs_write,
	.statfs = fs_statfs,
	.release = fs_release,
	.fsync = fs_fsync,
	.opendir = fs_opendir,
	.readdir = fs_readdir,
	.releasedir = fs_releasedir,
	.init = fs_init,
	.destroy = fs_destroy,
	.create = fs_create,
	.access = fs_access,
	.ftruncate = fs_ftruncate,
	.fgetattr = fs_fgetattr,
	.utimens = fs_utimens,
};
