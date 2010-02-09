#ifndef __IO_H__
#define __IO_H__

#include <sys/types.h>

struct io;

typedef ssize_t (*io_read_t)(struct io *, void *, size_t);
typedef ssize_t (*io_write_t)(struct io *, const void *, size_t);

union io_handle {
	void *ptr;
	int fd;
};

struct io {
	io_read_t read;
	io_write_t write;
	union io_handle handle;
};

#endif
