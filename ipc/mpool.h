#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <sys/types.h>
#include "avl.h"

struct mpool {
	struct avl mem;
};

void mpool_init(struct mpool *);
void mpool_cleanup(struct mpool *);
void *mpool_alloc(struct mpool *, size_t);
void *mpool_realloc(struct mpool *, void *, size_t);
void mpool_free(struct mpool *, void *);

#endif
