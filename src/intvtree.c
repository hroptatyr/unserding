/*** intvtree.c -- interval trees based on red-black trees
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "unserding-nifty.h"
#include "intvtree.h"
/* for thread-safe intvtrees */
#include <pthread.h>

#define MIN_KEY		0
#define MAX_KEY		-1U
#define NDSTK_SIZE	16

struct it_node_s {
	/* book-keeping */
	uint32_t key;
	uint32_t high;
	uint32_t max_high;
	bool redp;

	/* tree navigation */
	it_node_t left;
	it_node_t right;
	it_node_t parent;

	/* satellite data */
	void *data;
};

struct itree_s {
	struct it_node_s root;
	pthread_mutex_t mtx;
	char satellite[] __attribute__((aligned(__alignof(void*))));
};

static struct it_node_s __nil = {
	.key = MIN_KEY,
	.high = MIN_KEY,
	.max_high = MIN_KEY,
	.redp = false,
	.left = &__nil,
	.right = &__nil,
	.parent = &__nil,
	.data = NULL,
};
static it_node_t nil = &__nil;


/* nodes, ctor */
static void
init_node(it_node_t nd, uint32_t lo, uint32_t hi, void *data)
{
	memset(nd, 0, sizeof(*nd));
	nd->key = lo;
	nd->high = nd->max_high = hi;
	nd->data = data;

	nd->left = nd->right = nd->parent = nil;
	return;
}

static it_node_t
make_node(uint32_t lo, uint32_t hi, void *data)
{
	it_node_t n = xnew(struct it_node_s);
	init_node(n, lo, hi, data);
	return n;
}

static void
free_node(it_node_t in)
{
	memset(in, 0, sizeof(*in));
	free(in);
	return;
}

static inline int
max(int a, int b)
{
	return a > b ? a : b;
}

static inline bool
nil_node_p(it_node_t in)
{
	return in == nil;
}

static inline it_node_t
nil_node(void)
{
	return nil;
}

static inline it_node_t
itree_root_node(itree_t it)
{
	return &it->root;
}

static inline bool
itree_root_node_p(itree_t it, it_node_t in)
{
	return in == itree_root_node(it);
}

static inline it_node_t
itree_left_root(itree_t it)
{
	return itree_root_node(it)->left;
}

static inline uint32_t
max_high(it_node_t nd)
{
	return nd->max_high;
}

static inline uint32_t
children_max_high(it_node_t x)
{
	uint32_t xlh = max_high(x->left);
	uint32_t xrh = max_high(x->right);
	return max(xlh, xrh);
}

static bool
inner_node_p(it_node_t nd)
{
	return !(nil_node_p(nd->left) || nil_node_p(nd->right));
}


/* ctor */
static inline void
init_itree(itree_t it)
{
	init_node(itree_root_node(it), MAX_KEY, MAX_KEY, NULL);
	itree_root_node(it)->redp = false;
	pthread_mutex_init(&it->mtx, NULL);
	return;
}

itree_t
make_itree(void)
{
	itree_t res = xnew(struct itree_s);
	init_itree(res);
	return res;
}

itree_t
make_itree_sat(void *sat, size_t sat_size)
{
	itree_t res = malloc(sizeof(struct itree_s) + sat_size);
	init_itree(res);
	memcpy(res->satellite, sat, sat_size);
	return res;
}

void
free_itree(itree_t it)
{
	it_node_t x;

	pthread_mutex_lock(&it->mtx);
	x = itree_left_root(it);
	if (!nil_node_p(x)) {
#if 0
/* implement me */
		if (x->left != it->nil) {
			stuffToFree.Push(x->left);
		}
		if (x->right != nil) {
			stuffToFree.Push(x->right);
		}
		free_node(x);
		while (stuffToFree.NotEmpty()) {
			x = stuffToFree.Pop();
			if (x->left != it->nil) {
				stuffToFree.Push(x->left);
			}
			if (x->right != it->nil) {
				stuffToFree.Push(x->right);
			}
			free_node(x);
		}
#endif
	}
	memset(itree_root_node(it), 0, sizeof(struct it_node_s));
	pthread_mutex_unlock(&it->mtx);
	pthread_mutex_destroy(&it->mtx);
	return;
}

void*
itree_satellite(itree_t it)
{
	return it->satellite;
}

/* opers */
static void
itree_rot_left(itree_t UNUSED(it), it_node_t p)
{
	it_node_t y = p->right;

	p->right = y->left;

	if (!nil_node_p(y->left)) {
		y->left->parent = p;
	}
	y->parent = p->parent;

	if (p == p->parent->left) {
		p->parent->left = y;
	} else {
		p->parent->right = y;
	}
	y->left = p;
	p->parent = y;

	p->max_high = max(p->high, children_max_high(p));
	y->max_high = max(max_high(p), max(max_high(y->right), y->high));
	return;
}

static void
itree_rot_right(itree_t UNUSED(it), it_node_t p)
{
	it_node_t x = p->left;

	p->left = x->right;

	if (!nil_node_p(x->right)) {
		x->right->parent = p;
	}
	x->parent = p->parent;

	if (p == p->parent->left) {
		p->parent->left = x;
	} else {
		p->parent->right = x;
	}

	x->right = p;
	p->parent = x;

	p->max_high = max(p->high, children_max_high(p));
	x->max_high = max(max_high(x->left), max(max_high(p), x->high));
	return;
}

static void
itree_ins_help(itree_t it, it_node_t z)
{
	it_node_t x, y;
    
	z->left = z->right = nil_node();
	y = itree_root_node(it);
	x = itree_left_root(it);
	while (!nil_node_p(x)) {
		y = x;
		if (x->key > z->key) { 
			x = x->left;
		} else {
			x = x->right;
		}
	}
	z->parent = y;
	if ((y == itree_root_node(it)) || (y->key > z->key)) { 
		y->left = z;
	} else {
		y->right = z;
	}
	return;
}

static void
itree_fixup_max_high(itree_t it, it_node_t x)
{
	while (x != itree_root_node(it)) {
		x->max_high = max(x->high, children_max_high(x));
		x = x->parent;
	}
	return;
}

it_node_t
itree_add(itree_t it, uint32_t lo, uint32_t hi, void *data)
{
	it_node_t x, y, res;

	x = res = make_node(lo, hi, data);
	pthread_mutex_lock(&it->mtx);
	itree_ins_help(it, x);
	itree_fixup_max_high(it, x->parent);
	x->redp = true;
	while (x->parent->redp && x->parent->parent) {
		/* use sentinel instead of checking for root */
		if (x->parent == x->parent->parent->left) {
			y = x->parent->parent->right;
			if (y && y->redp) {
				x->parent->redp = false;
				y->redp = false;
				x->parent->parent->redp = true;
				x = x->parent->parent;
			} else {
				if (x == x->parent->right) {
					x = x->parent;
					itree_rot_left(it, x);
				}
				x->parent->redp = false;
				x->parent->parent->redp = true;
				itree_rot_right(it, x->parent->parent);
			}
		} else {
			/* case for x->parent == x->parent->parent->right */
			/* this part is just like the section above with */
			/* left and right interchanged */
			y = x->parent->parent->left;
			if (y && y->redp) {
				x->parent->redp = false;
				y->redp = false;
				x->parent->parent->redp = true;
				x = x->parent->parent;
			} else {
				if (x == x->parent->left) {
					x = x->parent;
					itree_rot_right(it, x);
				}
				x->parent->redp = false;
				x->parent->parent->redp = true;
				itree_rot_left(it, x->parent->parent);
			}
		}
	}
	if (!nil_node_p(x = itree_left_root(it))) {
		x->redp = false;
	}
	pthread_mutex_unlock(&it->mtx);
	return res;
}

it_node_t
itree_succ_of(itree_t it, it_node_t x)
{ 
	it_node_t y;

	pthread_mutex_lock(&it->mtx);
	if (!nil_node_p((y = x->right))) {
		/* get the minimum of the right subtree of x */
		while (!nil_node_p(y->left)) {
			y = y->left;
		}
		goto out;
	} else {
		y = x->parent;
		while (x == y->right) {
			x = y;
			y = y->parent;
		}
		if (y == itree_root_node(it)) {
			y = nil_node();
			goto out;
		}
		goto out;
	}
out:
	pthread_mutex_unlock(&it->mtx);
	return y;
}

it_node_t
itree_pred_of(itree_t it, it_node_t x)
{
	it_node_t y;

	pthread_mutex_lock(&it->mtx);
	if (!nil_node_p((y = x->left))) {
		while (!nil_node_p(y->right)) {
			/* returns the maximum of the left subtree of x */
			y = y->right;
		}
		goto out;
	} else {
		y = x->parent;
		while (x == y->left) { 
			if (y == itree_root_node(it)) {
				y = nil_node();
				goto out;
			}
			x = y;
			y = y->parent;
		}
		goto out;
	}
out:
	pthread_mutex_unlock(&it->mtx);
	return y;
}

static void
itree_del_fixup(itree_t it, it_node_t x)
{
	it_node_t rl;

	pthread_mutex_lock(&it->mtx);
	rl = itree_left_root(it);
	while ((!x->redp) && (rl != x)) {
		it_node_t w;
		if (x == x->parent->left) {
			w = x->parent->right;
			if (w->redp) {
				w->redp = false;
				x->parent->redp = true;
				itree_rot_left(it, x->parent);
				w = x->parent->right;
			}
			if ((!w->right->redp) && (!w->left->redp)) { 
				w->redp = true;
				x = x->parent;
			} else {
				if (!w->right->redp) {
					w->left->redp = false;
					w->redp = true;
					itree_rot_right(it, w);
					w = x->parent->right;
				}
				w->redp = x->parent->redp;
				x->parent->redp = false;
				w->right->redp = false;
				itree_rot_left(it, x->parent);
				break;
			}
		} else { 
			/* the code below has left and right
			 * switched from above */
			w = x->parent->left;
			if (w->redp) {
				w->redp = false;
				x->parent->redp = true;
				itree_rot_right(it, x->parent);
				w = x->parent->left;
			}
			if ((!w->right->redp) && (!w->left->redp)) { 
				w->redp = true;
				x = x->parent;
			} else {
				if (!w->left->redp) {
					w->right->redp = false;
					w->redp = true;
					itree_rot_left(it, w);
					w = x->parent->left;
				}
				w->redp = x->parent->redp;
				x->parent->redp = false;
				w->left->redp = false;
				itree_rot_right(it, x->parent);
				break;
			}
		}
	}
	x->redp = false;
	pthread_mutex_unlock(&it->mtx);
	return;
}

void*
itree_del_node(itree_t it, it_node_t z)
{
	it_node_t y, x;
	void *res = z->data;

	pthread_mutex_lock(&it->mtx);
	if (!inner_node_p(z)) {
		y = z;
	} else {
		y = itree_succ_of(it, z);
	}
	x = nil_node_p(y->left)
		? y->right
		: y->left;

	if (itree_root_node(it) == (x->parent = y->parent)) {
		itree_root_node(it)->left = x;

	} else {
		if (y == y->parent->left) {
			y->parent->left = x;
		} else {
			y->parent->right = x;
		}
	}
	if (y != z) {
		/* y should not be nil in this case */
		/* y is the node to splice out and x is its child */
		y->max_high = MIN_KEY;
		y->left = z->left;
		y->right = z->right;
		y->parent = z->parent;
		z->left->parent = z->right->parent = y;

		if (z == z->parent->left) {
			z->parent->left = y; 
		} else {
			z->parent->right = y;
		}
		itree_fixup_max_high(it, x->parent);

		if (!y->redp) {
			y->redp = z->redp;
			itree_del_fixup(it, x);
		} else {
			y->redp = z->redp;
		}
		free_node(z);
	} else {
		itree_fixup_max_high(it, x->parent);
		if (!y->redp) {
			itree_del_fixup(it, x);
		}
		free_node(y);
	}
	pthread_mutex_unlock(&it->mtx);
	return res;
}


/* printer shit */
static void __attribute__((noinline))
it_node_print(itree_t it, it_node_t in)
{
	printf("k=%u, h=%u, mh=%u", in->key, in->high, in->max_high);
	fputs("  l->key=", stdout);
	if (nil_node_p(in->left)) {
		fputs("NULL", stdout);
	} else {
		printf("%u", in->left->key);
	}
	fputs("  r->key=", stdout);
	if (nil_node_p(in->right)) {
		fputs("NULL", stdout);
	} else {
		printf("%u", in->right->key);
	}
	fputs("  p->key=", stdout);
	if (in->parent == itree_root_node(it)) {
		fputs("NULL", stdout);
	} else {
		printf("%u", in->parent->key);
	}
	printf("  red=%i\n", in->redp);
	return;
}

static void
itree_print_helper(itree_t it, it_node_t x)
{
	if (!nil_node_p(x)) {
		itree_print_helper(it, x->left);
		it_node_print(it, x);
		itree_print_helper(it, x->right);
	}
	return;
}

void
itree_print(itree_t it)
{
	itree_print_helper(it, itree_left_root(it));
	return;
}


/* iterators and node stack fiddlers*/
typedef struct it_ndstk_s *it_ndstk_t;
struct it_ndstk_s {
	index_t idx;
	it_node_t *stk;
};

static inline void
stack_push(it_ndstk_t stk, it_node_t nd)
{
	stk->stk[stk->idx++] = nd;
	return;
}

static inline it_node_t
stack_pop(it_ndstk_t stk)
{
	if (stk->idx == 0) {
		return nil_node();
	}
	return stk->stk[--stk->idx];
}

static inline it_node_t
stack_top(it_ndstk_t stk)
{
	if (stk->idx == 0) {
		return nil_node();
	}
	return stk->stk[stk->idx - 1];
}

static inline size_t
stack_size(it_ndstk_t stk)
{
	return stk->idx;
}

static void __attribute__((unused))
__itree_trav_pre_order(
	itree_t UNUSED(it), it_trav_f cb, void *clo, it_ndstk_t stk)
{
	while (stack_size(stk)) {
		it_node_t top = stack_pop(stk);
		if (!nil_node_p(top->right)) {
			stack_push(stk, top->right);
		}
		if (!nil_node_p(top->left)) {
			stack_push(stk, top->left);
		}
		cb(top->key, top->high, top->data, clo);
	}
	return;
}

void
itree_trav_in_order(itree_t it, it_trav_f cb, void *clo)
{
/* left child, me, right child */
	/* root node has no right child, proceed with the left one */
	it_node_t curr;
	it_node_t ____stk[128];
	struct it_ndstk_s __stk = {.idx = 0, .stk = ____stk}, *stk = &__stk;

#define proc(_x)						\
	do {							\
		it_node_t _y = _x;				\
		if (!nil_node_p(_y)) {				\
			cb(_y->key, _y->high, _y->data, clo);	\
		}						\
	} while (0)

	pthread_mutex_lock(&it->mtx);
	curr = itree_left_root(it);
	while (!nil_node_p(curr)) {
		if (inner_node_p(curr)) {
			stack_push(stk, curr->right);
			stack_push(stk, curr);
			curr = curr->left;

		} else {
			/* we just work off the shite, knowing there's
			 * a balance and the subtree consists of only
			 * one node */
			if (!nil_node_p(curr->left)) {
				proc(curr->left);
			}
			proc(curr);
			if (!nil_node_p(curr->right)) {
				proc(curr->right);
			}
			proc(stack_pop(stk));
			curr = stack_pop(stk);
		}
	}
#undef proc
	pthread_mutex_unlock(&it->mtx);
	return;
}

/* 0 if N contains P, 1 if P is right of N and -1 if N is right of P. */
static inline int
node_pivot_rel(it_node_t n, uint32_t p)
{
	if (p < n->key) {
		return -1;
	} else if (p > n->high) {
		return 1;
	} else {
		return 0;
	}
}

/* 0 if N contains P, 1 if P is right of N and -1 if N is right of P. */
static inline int
tree_pivot_rel(it_node_t n, uint32_t p)
{
	if (p < n->key) {
		return -1;
	} else if (p > max_high(n)) {
		return 1;
	} else {
		return 0;
	}
}

void
itree_find_point_cb(itree_t it, uint32_t p, it_trav_f cb, void *clo)
{
/* left child, me, right child */
	/* root node has no right child, proceed with the left one */
	it_node_t curr;
	it_node_t ____stk[128];
	struct it_ndstk_s __stk = {.idx = 0, .stk = ____stk}, *stk = &__stk;

#define proc(_x)						\
	do {							\
		it_node_t _y = _x;				\
		if (!nil_node_p(_y) &&				\
		    node_pivot_rel(_y, p) == 0) {		\
			cb(_y->key, _y->high, _y->data, clo);	\
		}						\
	} while (0)

	pthread_mutex_lock(&it->mtx);
	curr = itree_left_root(it);
	while (!nil_node_p(curr)) {
		switch (tree_pivot_rel(curr, p)) {
		case -1:
			/* if the pivot is truly to the left of curr, descend */
			curr = curr->left;
			continue;
		case 1:
			/* pivot is beyond the scope, ascend */
			proc(stack_pop(stk));
			curr = stack_pop(stk);
			continue;
		case 0:
		default:
			break;
		}
		/* we know now that curr's right subtree contains the pivot
		 * however the left tree could contain the pivot as well so
		 * descend there */
		if (inner_node_p(curr)) {
			if (tree_pivot_rel(curr->left, p) == 0) {
				stack_push(stk, curr->right);
				stack_push(stk, curr);
				curr = curr->left;
			} else {
				proc(curr);
				curr = curr->right;
			}

		} else {
			/* we just work off the shite, knowing there's
			 * a balance and the subtree consists of only
			 * one node */
			if (!nil_node_p(curr->left)) {
				proc(curr->left);
			}
			proc(curr);
			if (!nil_node_p(curr->right)) {
				proc(curr->right);
			}
			proc(stack_pop(stk));
			curr = stack_pop(stk);
		}
	}
#undef proc
	pthread_mutex_unlock(&it->mtx);
	return;
}

void
itree_find_point_cb1(itree_t it, uint32_t p, it_trav_f cb, void *clo)
{
/* like itree_find_point() but stop after one occurrence,
 * prefer the right branch for nebulous reasons */
	/* root node has no right child, proceed with the left one */
	it_node_t curr;

#define proc(_x)						\
	do {							\
		it_node_t _y = _x;				\
		cb(_y->key, _y->high, _y->data, clo);		\
	} while (0)

	pthread_mutex_lock(&it->mtx);
	curr = itree_left_root(it);
	while (!nil_node_p(curr)) {
		switch (tree_pivot_rel(curr, p)) {
		case -1:
			/* if the pivot is truly to the left of curr, descend */
			curr = curr->left;
			continue;
		case 1:
			/* pivot is beyond the scope, return */
			goto out;
		case 0:
		default:
			break;
		}

		if (node_pivot_rel(curr, p) == 0) {
			/* bingo, mother load */
			proc(curr);
			goto out;
		} else {
			/* this means the above was 1, -1 isn't possible here
			 * unless the machine is retarded */
			curr = curr->right;
		}
	}
#undef proc
out:
	pthread_mutex_unlock(&it->mtx);
	return;
}

void*
itree_find_point(itree_t it, uint32_t p)
{
/* like itree_find_point() but stop after one occurrence,
 * prefer the right branch for nebulous reasons */
	/* root node has no right child, proceed with the left one */
	it_node_t curr;
	void *data = NULL;

	pthread_mutex_lock(&it->mtx);
	curr = itree_left_root(it);
	while (!nil_node_p(curr)) {
		switch (tree_pivot_rel(curr, p)) {
		case -1:
			/* if the pivot is truly to the left of curr, descend */
			curr = curr->left;
			continue;
		case 1:
			/* pivot is beyond the scope, return */
			goto out;
		case 0:
		default:
			break;
		}

		if (node_pivot_rel(curr, p) == 0) {
			/* bingo, mother load */
			data = curr->data;
			goto out;
		} else {
			/* this means the above was 1, -1 isn't possible here
			 * unless the machine is retarded */
			curr = curr->right;
		}
	}
out:
	pthread_mutex_unlock(&it->mtx);
	return data;
}

/* intvtree.c ends here */
