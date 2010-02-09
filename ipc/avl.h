/*
 *               AVL Trees
 *
 * Example:
 *
 * struct datum {
 *	struct avl_node avl_internal;
 *	int key;
 *	int value;
 * };
 *
 * static int datum_cmp(const struct datum *x, const struct datum *y)
 * {
 *	return x->key - y->key;
 * }
 *
 * struct avl t;
 * avl_init(&t, offsetof(struct datum, avl_internal), (avl_cmp_t)datum_cmp);
 *
 * struct datum *new = malloc(sizeof(*new));
 * new->key = 17;
 * new->value = 42;
 * struct datum *old17 = avl_insert(&t, new);
 * free(old17);
 *
 * struct datum x = {.key = 42};
 * struct datum *x42 = avl_search(&t, &x);
 *
 * if (x42 != NULL) {
 *	struct datum y = {.key = x42->value};
 *	struct datum *yx = avl_remove(&t, &y);
 *	free(yx);
 * }
 *
 * avl_traverse(&t, (avl_process_t)free);
 */

#ifndef __AVL_H__
#define __AVL_H__

#include <stddef.h>

typedef int (*avl_cmp_t)(const void *, const void *);

struct avl {
	void *root;
	size_t offset;
	avl_cmp_t cmp;
};

struct avl_node {
	void *child[2];
	int balance;
};

void avl_init(struct avl *, size_t, avl_cmp_t);

void *avl_search(const struct avl *, const void *);
void *avl_insert(struct avl *, void *);
void *avl_remove(struct avl *, const void *);

typedef void (*avl_process_t)(void *);

void avl_traverse(const struct avl *, avl_process_t);

#endif
