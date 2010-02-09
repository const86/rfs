#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include "mpool.h"

static int cmp(const void *x, const void *y)
{
	return (x < y) ? -1 : (x > y) ? 1 : 0;
}

void mpool_init(struct mpool *mp)
{
	avl_init(&mp->mem, 0, cmp);
}

void mpool_cleanup(struct mpool *mp)
{
	avl_traverse(&mp->mem, free);
	mp->mem.root = NULL;
}

void *mpool_realloc(struct mpool *mp, void *p, size_t size)
{
	if (size == 0) {
		mpool_free(mp, p);
		return NULL;
	}

	if (p == NULL)
		return mpool_alloc(mp, size);

	struct avl_node *q = p;
	q--;
	avl_remove(&mp->mem, q);

	struct avl_node *r = realloc(q, sizeof(struct avl_node) + size);
	if (r != NULL) {
		avl_insert(&mp->mem, r);
		r++;
	}

	return r;
}

void *mpool_alloc(struct mpool *mp, size_t size)
{
	if (size == 0)
		return NULL;

	struct avl_node *p = malloc(sizeof(struct avl_node) + size);
	if (p != NULL) {
		avl_insert(&mp->mem, p);
		p++;
	}

	return p;
}

void mpool_free(struct mpool *mp, void *p)
{
	if (p == NULL)
		return;

	struct avl_node *q = p;
	q--;

	avl_remove(&mp->mem, q);
	free(q);
}
