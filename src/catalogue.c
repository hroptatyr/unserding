/*** catalogue-ng.c -- new generation catalogue
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"
#include "protocore-private.h"
#include "catalogue.h"
/* other external stuff */
#include <pfack/instruments.h>

#define INITIAL_SIZE	8192

typedef struct cat_s *_cat_t;

#define AS_CAT(x)	((_cat_t)(x))
#define xnew(x)		(malloc(sizeof(x)))

static inline bool
gaid_equal_p(gaid_t id1, gaid_t id2)
{
	return id1 == id2 && id1 != 0;
}

/* ugly? */
#define INSTR_NAME_SZ	32
static inline bool
name_equal_p(const char *n1, const char *n2)
{
	return strncmp(n1, n2, sizeof(INSTR_NAME_SZ)) == 0;
}

static inline instr_t
cat_instr(cat_t cat, index_t i)
{
	_cat_t c = cat;
	instr_t instrs = c->instrs;
	return &instrs[i];
}

/* state-of-the-art hash seems to be murmur2, so let's adapt it for gaids */
static uint32_t
murmur2(gaid_t key)
{
	/* 'm' and 'r' are mixing constants generated offline.
	 * They're not really 'magic', they just happen to work well. */
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	/* initialise to a random value, originally passed as `seed'*/
#define seed	137173456
	unsigned int h = seed;

	key *= m;
	key ^= key >> r;
	key *= m;

	h *= m;
	h ^= key;

	/* Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}

static uint32_t
slot(struct keyval_s *keys, size_t size, gaid_t key)
{
/* return the first slot in c->keys that either contains gaid or would
 * be a warm n cuddly place for it */
	uint32_t res = murmur2(key) % size;
	for (uint32_t i = res; i < size; i++) {
		if (keys[i].key == 0) {
			return i;
		} else if (gaid_equal_p(keys[i].key, key)) {
			return i;
		}
	}
	/* rotate round */
	for (uint32_t i = 0; i < res; i++) {
		if (keys[i].key == 0) {
			return i;
		} else if (gaid_equal_p(keys[i].key, key)) {
			return i;
		}
	}
	/* means we're full :O */
	return -1;
}

static inline uint32_t
get_val(_cat_t c, gaid_t key)
{
	uint32_t s = slot(c->keys, c->alloc_sz, key);

	if (c->keys[s].key != 0) {
		return c->keys[s].val;
	}
	return -1;
}

static inline void
add_key(_cat_t c, gaid_t key, uint32_t idx)
{
	uint32_t s = slot(c->keys, c->alloc_sz, key);
	c->keys[s].val = idx;
	return;
}

static void
resize_keys(_cat_t c, size_t old_sz, size_t new_sz)
{
	size_t new_msz = new_sz * sizeof(*c->keys);
	struct keyval_s *new = malloc(new_msz);

	memset(new, 0, new_msz);
	for (uint32_t i = 0; i < old_sz; i++) {
		uint32_t new_s;

		if (c->keys[i].key == 0) {
			continue;
		}
		new_s = slot(new, new_sz, c->keys[i].key);
		new[new_s].key = c->keys[i].key;
		new[new_s].val = c->keys[i].val;
	}
	free(c->keys);
	/* assign the new one */
	c->keys = new;
	return;
}

static void
init_keys(_cat_t c, size_t ini_sz)
{
	size_t msz = ini_sz * sizeof(*c->keys);
	c->keys = malloc(msz);
	memset(c->keys, 0, msz);
	return;
}

static void
free_keys(_cat_t c)
{
	free(c->keys);
	return;
}

static instr_t
__by_gaid_nolock(_cat_t c, gaid_t gaid)
{
	/* obtain the slot into our instrs array */
	uint32_t s = get_val(c, gaid);

	if (s == -1U) {
		/* amazing, never seen this instr */
		return NULL;
	}
	return cat_instr(c, s);
}

static instr_t
__by_name_nolock(_cat_t c, const char *name)
{
	for (index_t i = 0; i < c->ninstrs; i++) {
		instr_t this_instr = cat_instr(c, i);
		ident_t this_ident = instr_ident(this_instr);
		if (ident_name(this_ident)[0] != '\0' &&
		    name_equal_p(ident_name(this_ident), name)) {
			pthread_mutex_unlock(&c->mtx);
			return this_instr;
		}
	}
	return NULL;
}


/* ctor, dtor */
cat_t
make_cat(void)
{
	_cat_t res = xnew(struct cat_s);

	pthread_mutex_init(&res->mtx, NULL);
	res->instrs = malloc(sizeof(*res->instrs) * INITIAL_SIZE);
	init_keys(res, INITIAL_SIZE);
	res->alloc_sz = INITIAL_SIZE;
	res->ninstrs = 0;
	return res;
}

void
free_cat(cat_t cat)
{
	_cat_t c = cat;

	pthread_mutex_lock(&c->mtx);
	pthread_mutex_unlock(&c->mtx);
	pthread_mutex_destroy(&c->mtx);

	free(c->instrs);
	free_keys(c);
	free(c);
	return;
}

/* modifiers */
void
cat_add_instr(cat_t cat, instr_t instr)
{
	return;
}

size_t
cat_size(cat_t cat)
{
	_cat_t c = cat;
	size_t res;

	pthread_mutex_lock(&c->mtx);
	res = AS_CAT(cat)->ninstrs;
	pthread_mutex_unlock(&c->mtx);
	return res;
}

static inline bool
check_resize(_cat_t c)
{
	if (UNLIKELY(c->ninstrs == c->alloc_sz)) {
		/* resize */
		size_t old_sz = c->alloc_sz;
		size_t new_sz = c->alloc_sz * 2;
		c->instrs = realloc(c->instrs, new_sz * sizeof(*c->instrs));
		resize_keys(c, old_sz, new_sz);
		c->alloc_sz = new_sz;
		/* indicate that we did resize */
		return true;
	}
	/* not resized, all slots should be intact */
	return false;
}

/* doesnt work well with the keys schema we invented */
instr_t
cat_obtain_instr(cat_t cat)
{
/* returns the next available instrument */
	_cat_t c = cat;
	instr_t res;

	pthread_mutex_lock(&c->mtx);
	check_resize(c);
	res = cat_instr(c, c->ninstrs++);
	pthread_mutex_unlock(&c->mtx);
	return res;
}

instr_t
cat_bang_instr(cat_t cat, instr_t i)
{
/* returns the next instrument we've banged into */
	_cat_t c = cat;
	instr_t res;
	gaid_t gaid = instr_gaid(i);
	/* key slot and instr slot */
	uint32_t ks, is;

	pthread_mutex_lock(&c->mtx);
	ks = slot(c->keys, c->alloc_sz, gaid);
	if (UNLIKELY(ks != -1U && c->keys[ks].key != 0)) {
		res = cat_instr(c, c->keys[ks].val);
		pthread_mutex_unlock(&c->mtx);
		return res;
	}
	if (check_resize(c)) {
		ks = slot(c->keys, c->alloc_sz, gaid);
	}
	is = c->ninstrs++;
	c->keys[ks].key = gaid;
	c->keys[ks].val = is;
	res = cat_instr(c, is);
	memcpy(res, i, sizeof(*i));
	pthread_mutex_unlock(&c->mtx);
	return res;
}

instr_t
find_instr_by_gaid(cat_t cat, gaid_t gaid)
{
	instr_t res;
	_cat_t c = cat;

	pthread_mutex_lock(&c->mtx);
	res = __by_gaid_nolock(c, gaid);
	pthread_mutex_unlock(&c->mtx);
	return res;
}

instr_t
find_instr_by_name(cat_t cat, const char *name)
{
	instr_t res;
	_cat_t c = cat;

	pthread_mutex_lock(&c->mtx);
	res = __by_name_nolock(c, name);
	pthread_mutex_unlock(&c->mtx);
	return res;
}

/* catalogue-ng.c ends here */
