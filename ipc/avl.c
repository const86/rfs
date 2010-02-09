#define _XOPEN_SOURCE 600

#include <stdbool.h>
#include "avl.h"

void avl_init(struct avl *t, size_t offset, avl_cmp_t cmp)
{
	t->root = NULL;
	t->offset = offset;
	t->cmp = cmp;
}

static inline struct avl_node *node(const struct avl *t, void *p)
{
	return (struct avl_node *)((char *)p + t->offset);
}

static void traverse(const struct avl *t, void *p, avl_process_t process)
{
	if (p == NULL)
		return;

	void *q = node(t, p)->child[1];

	traverse(t, node(t, p)->child[0], process);
	process(p);
	traverse(t, q, process);
}

void avl_traverse(const struct avl *t, avl_process_t process)
{
	traverse(t, t->root, process);
}

void *avl_search(const struct avl *t, const void *x)
{
	for (void *p = t->root; p != NULL;) {
		int cmp = t->cmp(x, p);

		if (cmp < 0)
			p = node(t, p)->child[0];
		else if (cmp == 0)
			return p;
		else
			p = node(t, p)->child[1];
	}

	return NULL;
}

static inline void rotate(const struct avl *t, void **pp, bool dir)
{
	void *p = *pp;
	void *r = node(t, p)->child[dir];

	node(t, p)->child[dir] = node(t, r)->child[!dir];
	node(t, r)->child[!dir] = p;
	*pp = r;

	if (node(t, r)->balance == 0) {
		node(t, p)->balance = dir ? 1 : -1;
		node(t, r)->balance = dir ? -1 : 1;
	} else
		node(t, p)->balance = node(t, r)->balance = 0;
}

static inline void rotate2(const struct avl *t, void **pp, bool dir)
{
	void *p = *pp;
	void *q = node(t, p)->child[dir];
	void *r = node(t, q)->child[!dir];

	*pp = r;
	node(t, p)->child[dir] = node(t, r)->child[!dir];
	node(t, r)->child[!dir] = p;
	node(t, q)->child[!dir] = node(t, r)->child[dir];
	node(t, r)->child[dir] = q;

	int idir = dir ? 1 : -1;
	int rb = node(t, r)->balance;

	node(t, node(t, r)->child[dir])->balance = (rb == -idir) ? idir : 0;
	node(t, node(t, r)->child[!dir])->balance = (rb == idir) ? -idir : 0;
	node(t, r)->balance = 0;
}

static inline bool insert_here(const struct avl *t, void **pp, void **px)
{
	struct avl_node *x = node(t, *px);

	if (*pp != NULL) {
		void *p = *pp;

		*node(t, *px) = *node(t, p);
		*pp = *px;
		*px = p;

		return false;
	}

	x->child[0] = x->child[1] = NULL;
	x->balance = 0;
	*pp = *px;
	*px = NULL;

	return true;
}

static bool insert(const struct avl *t, void **pp, void **px)
{
	if (*pp == NULL)
		return insert_here(t, pp, px);

	int cmp = t->cmp(*px, *pp);
	if (cmp == 0)
		return insert_here(t, pp, px);

	struct avl_node *p = node(t, *pp);
	bool dir = cmp > 0;
	int idir = dir ? 1 : -1;
	bool grow = insert(t, &p->child[dir], px);

	p->balance += idir * grow;
	if (!grow || p->balance == 0)
		return false;

	if (p->balance == idir)
		return true;

	if (node(t, p->child[dir])->balance == idir)
		rotate(t, pp, dir);
	else
		rotate2(t, pp, dir);

	return false;
}

void *avl_insert(struct avl *t, void *x)
{
	insert(t, &t->root, &x);
	return x;
}

static inline bool remove_fix(struct avl *t, void **pp, bool dir, bool shrink)
{
	struct avl_node *p = node(t, *pp);
	int idir = dir ? 1 : -1;

	p->balance -= idir * shrink;
	if (!shrink || p->balance == -idir)
		return false;

	if (p->balance == 0)
		return true;

	if (node(t, p->child[!dir])->balance == idir) {
		rotate2(t, pp, !dir);
		return true;
	} else {
		rotate(t, pp, !dir);
		return node(t, *pp)->balance == 0;
	}
}

static bool remove_swap(struct avl *t, void **pp, bool dir, void **swap)
{
	struct avl_node *p = node(t, *pp);

	if (p->child[dir] == NULL) {
		*swap = *pp;
		*pp = p->child[!dir];
		return true;
	}

	bool shrink = remove_swap(t, &p->child[dir], dir, swap);
	return remove_fix(t, pp, dir, shrink);
}

static bool remove(struct avl *t, void **pp, void **px)
{
	if (*pp == NULL) {
		*px = NULL;
		return false;
	}

	int cmp = t->cmp(*px, *pp);
	struct avl_node *p = node(t, *pp);
	bool dir, shrink;

	if (cmp == 0) {
		*px = *pp;
		dir = p->balance > 0;

		if (p->child[dir] == NULL) {
			*pp = p->child[!dir];
			return true;
		}

		void *swap;
		shrink = remove_swap(t, &p->child[dir], !dir, &swap);
		*node(t, swap) = *node(t, *pp);
		*pp = swap;
	} else {
		dir = cmp > 0;
		shrink = remove(t, &node(t, *pp)->child[dir], px);
	}

	return remove_fix(t, pp, dir, shrink);
}

void *avl_remove(struct avl *t, const void *x)
{
	remove(t, &t->root, (void **)&x);
	return (void *)x;
}
