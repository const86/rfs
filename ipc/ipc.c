#define _XOPEN_SOURCE 600

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"

void ipc_init(struct ipc *ipc)
{
	mpool_init(&ipc->mp);
	ipc->rb.pos = 0;
	ipc->rb.size = 0;
	ipc->wb.pos = 0;
}

bool ipc_read(struct ipc *ipc, void *p, size_t size)
{
	struct read_buffer *rb = &ipc->rb;

	while (size > 0) {
		if (rb->pos >= rb->size) {
			ssize_t done = ipc->io.read(&ipc->io, rb->data,
						IPC_BUFFER_SIZE);
			if (done <= 0)
				return false;

			rb->size = done;
			rb->pos = 0;
		}

		size_t chunk = size;
		if (rb->pos + size > rb->size)
			chunk = rb->size - rb->pos;

		memcpy(p, rb->data + rb->pos, chunk);
		p = (char *)p + chunk;
		size -= chunk;
		rb->pos += chunk;
	}

	return true;
}

bool ipc_write(struct ipc *ipc, const void *p, size_t size)
{
	struct write_buffer *wb = &ipc->wb;

	while (size > 0) {
		if (wb->pos >= IPC_BUFFER_SIZE) {
			ssize_t sent = ipc->io.write(&ipc->io, wb->data,
						     IPC_BUFFER_SIZE);
			if (sent < IPC_BUFFER_SIZE)
				return false;
			wb->pos = 0;
		}

		size_t chunk = size;
		if (wb->pos + size > IPC_BUFFER_SIZE)
			chunk = IPC_BUFFER_SIZE - wb->pos;

		memcpy(wb->data + wb->pos, p, chunk);
		p = (char *)p + chunk;
		size -= chunk;
		wb->pos += chunk;
	}

	return true;
}

bool ipc_flush(struct ipc *ipc)
{
	struct write_buffer *wb = &ipc->wb;

	if (wb->pos == 0)
		return true;

	if (ipc->io.write(&ipc->io, wb->data, wb->pos) < (ssize_t)wb->pos)
		return false;

	wb->pos = 0;
	return true;
}

bool ipc_read_uint32_t(struct ipc *ipc, uint32_t *p)
{
	uint32_t x;

	if (!ipc_read(ipc, &x, sizeof(x)))
		return false;

	*p = ntohl(x);
	return true;
}

bool ipc_write_uint32_t(struct ipc *ipc, const uint32_t *p)
{
	uint32_t x = htonl(*p);
	return ipc_write(ipc, &x, sizeof(x));
}

bool ipc_read_int32_t(struct ipc *ipc, int32_t *p)
{
	uint32_t x;
	if (!ipc_read_uint32_t(ipc, &x))
		return false;

	*p = x;
	return true;
}

bool ipc_write_int32_t(struct ipc *ipc, const int32_t *p)
{
	uint32_t x = *p;
	return ipc_write_uint32_t(ipc, &x);
}

bool ipc_read_uint64_t(struct ipc *ipc, uint64_t *p)
{
	uint32_t h, l;

	if (!ipc_read_uint32_t(ipc, &h) || !ipc_read_uint32_t(ipc, &l))
		return false;

	*p = ((uint64_t)h << 32) | l;
	return true;
}

bool ipc_write_uint64_t(struct ipc *ipc, const uint64_t *p)
{
	uint32_t h = *p >> 32, l = *p;
	return ipc_write_uint32_t(ipc, &h) && ipc_write_uint32_t(ipc, &l);
}

bool ipc_read_int64_t(struct ipc *ipc, int64_t *p)
{
	uint64_t x;

	if (!ipc_read_uint64_t(ipc, &x))
		return false;

	*p = x;
	return true;
}

bool ipc_write_int64_t(struct ipc *ipc, const int64_t *p)
{
	uint64_t x = *p;
	return ipc_write_uint64_t(ipc, &x);
}

bool ipc_read_datum(struct ipc *ipc, datum *p)
{
	if (!ipc_read_uint32_t(ipc, &p->n))
		return false;

	if (p->n == 0) {
		p->p = NULL;
		return true;
	}

	p->p = mpool_alloc(&ipc->mp, p->n);
	if (p->p == NULL)
		return false;

	return ipc_read(ipc, p->p, p->n);
}

bool ipc_write_datum(struct ipc *ipc, const datum *p)
{
	return ipc_write_uint32_t(ipc, &p->n) && ipc_write(ipc, p->p, p->n);
}

bool ipc_read_string(struct ipc *ipc, string *p)
{
	uint32_t len;

	if (!ipc_read_uint32_t(ipc, &len))
		return false;

	if (len == UINT32_MAX) {
		p->s = NULL;
		return true;
	}

	p->s = mpool_alloc(&ipc->mp, len + 1);
	if (p->s == NULL)
		return false;

	p->s[len] = '\0';
	return ipc_read(ipc, p->s, len);
}

bool ipc_write_string(struct ipc *ipc, const string *p)
{
	if (p->s == NULL) {
		uint32_t len32 = UINT32_MAX;
		return ipc_write_uint32_t(ipc, &len32);
	}

	size_t len = strlen(p->s);

	if (sizeof(len) > sizeof(uint32_t) && len > UINT32_MAX)
		return false;

	uint32_t len32 = len;
	return ipc_write_uint32_t(ipc, &len32) && ipc_write(ipc, p->s, len);
}
