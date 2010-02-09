#ifndef __IPC_H__
#define __IPC_H__

#include <stdbool.h>
#include <stdint.h>
#include "mpool.h"
#include "io.h"

#define IPC_BUFFER_SIZE (32 << 10)

struct read_buffer {
	uint8_t data[IPC_BUFFER_SIZE];
	size_t size;
	size_t pos;
};

struct write_buffer {
	uint8_t data[IPC_BUFFER_SIZE];
	size_t pos;
};

struct ipc {
	bool ok;
	struct io io;
	struct mpool mp;
	struct read_buffer rb;
	struct write_buffer wb;
};

void ipc_init(struct ipc *);

bool ipc_read(struct ipc *, void *, size_t);
bool ipc_write(struct ipc *, const void *, size_t);
bool ipc_flush(struct ipc *);

bool ipc_read_uint32_t(struct ipc *, uint32_t *);
bool ipc_write_uint32_t(struct ipc *, const uint32_t *);

bool ipc_read_int32_t(struct ipc *, int32_t *);
bool ipc_write_int32_t(struct ipc *, const int32_t *);

bool ipc_read_uint64_t(struct ipc *, uint64_t *);
bool ipc_write_uint64_t(struct ipc *, const uint64_t *);

bool ipc_read_int64_t(struct ipc *, int64_t *);
bool ipc_write_int64_t(struct ipc *, const int64_t *);

struct datum {
	uint32_t n;
	void *p;
};

typedef struct datum datum;

bool ipc_read_datum(struct ipc *, datum *);
bool ipc_write_datum(struct ipc *, const datum *);

union string {
	char *s;
	const char *cs;
};

typedef union string string;

bool ipc_read_string(struct ipc *, string *);
bool ipc_write_string(struct ipc *, const string *);

#endif
