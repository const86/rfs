#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <unistd.h>
#include "io_file.h"

static ssize_t io_file_read(struct io *io, void *p, size_t len)
{
	return read(io->handle.fd, p, len);
}

static ssize_t io_file_write(struct io *io, const void *p, size_t len)
{
	return write(io->handle.fd, p, len);
}

void io_file_init(struct io *io, int fd)
{
	io->handle.fd = fd;
	io->read = io_file_read;
	io->write = io_file_write;
}
