/*** tscube.c -- time series cube
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

#include <stdbool.h>
#include <stdint.h>
#include <sushi/secu.h>
#include <sushi/scommon.h>
#include "tscube.h"
#include "unserding-nifty.h"

#define INITIAL_SIZE	1024

typedef struct tscube_s *__tscube_t;
typedef struct tsc_key_s key_s;
typedef struct __val_s val_s;
typedef key_s *tsc_key_t;
typedef val_s *tsc_val_t;

struct __val_s {
	/* we should point to our interval tree here */
	void *intv;
	/* user's value */
	void *uval;
};

/**
 * Just a reverse lookup structure. */
struct keyval_s {
	/* user provided key */
	key_s key[1];
	/* lookup value */
	val_s val[1];
};

/**
 * The time series cube data structure.
 * Naive, just an array of time series interval trees, used as hash table
 * per the keys vector which gives a primitive lookup from
 * the hashing key to the index in the former vector. */
struct tscube_s {
	size_t nseries;
	size_t alloc_sz;
	struct keyval_s *tbl;
	pthread_mutex_t mtx;
};


/* helpers */
static inline bool
__key_equal_p(tsc_key_t id1, tsc_key_t id2)
{
	return id1 == id2;
}

static inline bool
__key_valid_p(tsc_key_t id1)
{
	return id1 != NULL;
}

static void
init_tbl(__tscube_t c, size_t ini_sz)
{
	size_t msz = ini_sz * sizeof(*c->tbl);
	c->tbl = xmalloc(msz);
	memset(c->tbl, 0, msz);
	return;
}

static void
free_tbl(__tscube_t c)
{
	for (index_t i = 0; i < c->alloc_sz; i++) {
		if (__key_valid_p(c->tbl[i].key)) {
#if 0
			free_tscoll(c->tbl[i].val);
#endif
		}
	}
	xfree(c->tbl);
	return;
}

#if 0
/* state-of-the-art hash seems to be murmur2, so let's adapt it for gaids */
static uint32_t
murmur2(secukey_t key)
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
#endif

#define SLOTS_FULL	((uint32_t)0xffffffff)
static uint32_t
slot(struct keyval_s *tbl, size_t size, tsc_key_t key)
{
/* return the first slot in c->tbl that either contains gaid or would
 * be a warm n cuddly place for it
 * linear probing */
	uint32_t res = 0; //murmur2(key) % size;
	for (uint32_t i = res; i < size; i++) {
		if (!__key_valid_p(tbl[i].key)) {
			return i;
		} else if (__key_equal_p(tbl[i].key, key)) {
			return i;
		}
	}
	/* rotate round */
	for (uint32_t i = 0; i < res; i++) {
		if (!__key_valid_p(tbl[i].key)) {
			return i;
		} else if (__key_equal_p(tbl[i].key, key)) {
			return i;
		}
	}
	/* means we're full :O */
	return SLOTS_FULL;
}

static void
resize_tbl(__tscube_t c, size_t old_sz, size_t new_sz)
{
	size_t new_msz = new_sz * sizeof(*c->tbl);
	struct keyval_s *new = malloc(new_msz);

	memset(new, 0, new_msz);
	for (uint32_t i = 0; i < old_sz; i++) {
		uint32_t new_s;

		if (!__key_valid_p(c->tbl[i].key)) {
			continue;
		}
		new_s = slot(new, new_sz, c->tbl[i].key);
		*new[new_s].key = *c->tbl[i].key;
		*new[new_s].val = *c->tbl[i].val;
	}
	free(c->tbl);
	/* assign the new one */
	c->tbl = new;
	return;
}

static inline bool
check_resize(__tscube_t c)
{
	if (UNLIKELY(c->nseries == c->alloc_sz)) {
		/* resize */
		size_t old_sz = c->alloc_sz;
		size_t new_sz = c->alloc_sz * 2;
		resize_tbl(c, old_sz, new_sz);
		c->alloc_sz = new_sz;
		/* indicate that we did resize */
		return true;
	}
	/* not resized, all slots should be intact */
	return false;
}


/* ctor, dtor */
tscache_t
make_tscube(void)
{
	__tscube_t res = xnew(*res);

	pthread_mutex_init(&res->mtx, NULL);
	init_tbl(res, INITIAL_SIZE);
	res->alloc_sz = INITIAL_SIZE;
	res->nseries = 0;
	return (void*)res;
}

void
free_tscube(tscube_t tsc)
{
	struct tscube_s *c = tsc;

	pthread_mutex_lock(&c->mtx);
	pthread_mutex_unlock(&c->mtx);
	pthread_mutex_destroy(&c->mtx);

	free_tbl(c);
	xfree(c);
	return;
}

#if 0
/* modifiers */
size_t
tscache_size(tscache_t tsc)
{
	__tscube_t c = tsc;
	size_t res;

	pthread_mutex_lock(&c->mtx);
	res = c->nseries;
	pthread_mutex_unlock(&c->mtx);
	return res;
}

tscoll_t
find_tscoll_by_secu(tscache_t tsc, su_secu_t secu)
{
	tscoll_t res = NULL;
	__tscube_t c = tsc;
	uint32_t ks;

	pthread_mutex_lock(&c->mtx);
	ks = slot(c->tbl, c->alloc_sz, secukey_from_secu(secu));
	if (LIKELY(ks != SLOTS_FULL && secukey_valid_p(c->tbl[ks].key))) {
		res = c->tbl[ks].val;
	}
	pthread_mutex_unlock(&c->mtx);
	return res;
}
#endif

void
tsc_add(tscube_t tsc, tsc_key_t key, void *val)
{
	__tscube_t c = tsc;
	uint32_t ks;

	pthread_mutex_lock(&c->mtx);
	ks = slot(c->tbl, c->alloc_sz, key);
	if (UNLIKELY(ks == SLOTS_FULL || !__key_valid_p(c->tbl[ks].key))) {
		/* means the slot is there, but it hasnt got a coll
		 * associated */
		(void)check_resize(c);
		ks = slot(c->tbl, c->alloc_sz, key);
		*c->tbl[ks].key = *key;
		/* create an interval tree, TODO */
		c->tbl[ks].val->intv = NULL; //make_tscoll(secu);
		/* assign user's idea of this */
		c->tbl[ks].val->uval = val;
		c->nseries++;
	}
	pthread_mutex_unlock(&c->mtx);
	return;
}

/* tscube.c ends here */
