#include <inttypes.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "rfs.h"

struct ipc ipc;
const struct fuse_operations fs_ops;

bool rfs_recover(uint64_t last_key);
void rfs_destroy(void);
