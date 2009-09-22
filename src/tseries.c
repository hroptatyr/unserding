/*** tseries.c -- stuff that is soon to be replaced by ffff's tseries
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

/* our master include */
#include "unserding.h"
#include "unserding-nifty.h"
#include "tseries.h"

typedef struct tscache_s *_tscache_t;
typedef uint32_t secukey_t;

#define INITIAL_SIZE	1024

/**
 * Just a reverse lookup structure. */
struct keyval_s {
	/** official, external id of the instrument */
	secukey_t key;
	/** local index in our table */
	uint32_t val;
};

/**
 * The time series cache data structure.
 * Naive, just an array of instruments, used as hash table. */
struct tscache_s {
	size_t nseries;
	size_t alloc_sz;
	struct tseries_s *series;
	struct keyval_s *keys;
	pthread_mutex_t mtx;
};


/* helpers */
static inline bool
secukey_equal_p(secukey_t id1, secukey_t id2)
{
	return id1 == id2 && id1 != 0;
}

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

static uint32_t
slot(struct keyval_s *keys, size_t size, secukey_t key)
{
/* return the first slot in c->keys that either contains gaid or would
 * be a warm n cuddly place for it */
	uint32_t res = murmur2(key) % size;
	for (uint32_t i = res; i < size; i++) {
		if (keys[i].key == 0) {
			return i;
		} else if (secukey_equal_p(keys[i].key, key)) {
			return i;
		}
	}
	/* rotate round */
	for (uint32_t i = 0; i < res; i++) {
		if (keys[i].key == 0) {
			return i;
		} else if (secukey_equal_p(keys[i].key, key)) {
			return i;
		}
	}
	/* means we're full :O */
	return -1;
}

static inline tseries_t
tscache_series(_tscache_t c, index_t i)
{
	tseries_t s = c->series;
	return &s[i];
}

static void
init_keys(_tscache_t c, size_t ini_sz)
{
	size_t msz = ini_sz * sizeof(*c->keys);
	c->keys = malloc(msz);
	memset(c->keys, 0, msz);
	return;
}

static void
free_keys(_tscache_t c)
{
	free(c->keys);
	return;
}

static inline uint32_t
get_val(_tscache_t c, secukey_t key)
{
	uint32_t s = slot(c->keys, c->alloc_sz, key);

	if (s != -1U && c->keys[s].key != 0) {
		return c->keys[s].val;
	}
	return -1;
}

static inline void
add_key(_tscache_t c, secukey_t key, uint32_t idx)
{
	uint32_t s = slot(c->keys, c->alloc_sz, key);
	c->keys[s].val = idx;
	return;
}

static void
resize_keys(_tscache_t c, size_t old_sz, size_t new_sz)
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


/* ctor, dtor */
tscache_t
make_tscache(void)
{
	_tscache_t res = xnew(struct tscache_s);

	pthread_mutex_init(&res->mtx, NULL);
	res->series = malloc(sizeof(*res->series) * INITIAL_SIZE);
	init_keys(res, INITIAL_SIZE);
	res->alloc_sz = INITIAL_SIZE;
	res->nseries = 0;
	return res;
}

void
free_tscache(tscache_t tsc)
{
	_tscache_t c = tsc;

	pthread_mutex_lock(&c->mtx);
	pthread_mutex_unlock(&c->mtx);
	pthread_mutex_destroy(&c->mtx);

	free(c->series);
	free_keys(c);
	free(c);
	return;
}

/* modifiers */
size_t
tscache_size(tscache_t tsc)
{
	_tscache_t c = tsc;
	size_t res;

	pthread_mutex_lock(&c->mtx);
	res = c->nseries;
	pthread_mutex_unlock(&c->mtx);
	return res;
}

static inline bool
check_resize(_tscache_t c)
{
	if (UNLIKELY(c->nseries == c->alloc_sz)) {
		/* resize */
		size_t old_sz = c->alloc_sz;
		size_t new_sz = c->alloc_sz * 2;
		c->series = realloc(c->series, new_sz * sizeof(*c->series));
		resize_keys(c, old_sz, new_sz);
		c->alloc_sz = new_sz;
		/* indicate that we did resize */
		return true;
	}
	/* not resized, all slots should be intact */
	return false;
}


tseries_t
tscache_bang_series(tscache_t tsc, tseries_t s)
{
/* returns the next instrument we've banged into */
	_tscache_t c = tsc;
	tseries_t res;
	secukey_t sk = s->kacke;
	/* key slot and instr slot */
	uint32_t ks, is;

	pthread_mutex_lock(&c->mtx);
	(void)check_resize(c);
	ks = slot(c->keys, c->alloc_sz, sk);
	if (UNLIKELY(c->keys[ks].key != 0)) {
		res = (void*)tscache_series(c, c->keys[ks].val);
		pthread_mutex_unlock(&c->mtx);
		return res;
	}
	is = c->nseries++;
	c->keys[ks].key = sk;
	c->keys[ks].val = is;
	res = (void*)tscache_series(c, is);
	memcpy(res, s, sizeof(*s));
	pthread_mutex_unlock(&c->mtx);
	return res;
}

tseries_t
find_tseries_by_secu(tscache_t tsc, secu_t secu)
{
	tseries_t res;
	_tscache_t c = tsc;
	uint32_t ks;

	pthread_mutex_lock(&c->mtx);
	ks = slot(c->keys, c->alloc_sz, secu->instr);
	if (UNLIKELY(ks == -1U)) {
		res = NULL;
	} else if (LIKELY(c->keys[ks].key != 0)) {
		uint32_t is = c->keys[ks].val;
		res = (void*)tscache_series(c, is);
	} else {
		res = NULL;
	}
	pthread_mutex_unlock(&c->mtx);
	return res;
}

/* tseries.c ends here */
