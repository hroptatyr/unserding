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
#include "intvtree.h"

typedef struct tscache_s *_tscache_t;
typedef uint32_t secukey_t;

#define INITIAL_SIZE	1024

/**
 * Just a reverse lookup structure. */
struct keyval_s {
	/** official, external id of the instrument */
	secukey_t key;
	/** local index in our table */
	void *val;
};

/**
 * The time series cache data structure.
 * Naive, just an array of time series, used as hash table
 * per the keys vector which gives a primitive lookup from
 * the hashing key to the index in the former vector. */
struct tscache_s {
	size_t nseries;
	size_t alloc_sz;
	struct keyval_s *tbl;
	pthread_mutex_t mtx;
};

/**
 * Like tseries but with annotation goodness. */
typedef struct anno_tseries_s *anno_tseries_t;
struct anno_tseries_s {
	struct tseries_s tseries;
	struct ts_anno_s anno;
};


/* helpers */
static inline bool
secukey_valid_p(secukey_t k)
{
	return k != 0;
}

static inline bool
secukey_equal_p(secukey_t id1, secukey_t id2)
{
	return id1 == id2 && secukey_valid_p(id1);
}

static inline secukey_t
secukey_from_secu(secu_t s)
{
	return s->instr;
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
slot(struct keyval_s *tbl, size_t size, secukey_t key)
{
/* return the first slot in c->tbl that either contains gaid or would
 * be a warm n cuddly place for it */
	uint32_t res = murmur2(key) % size;
	for (uint32_t i = res; i < size; i++) {
		if (tbl[i].key == 0) {
			return i;
		} else if (secukey_equal_p(tbl[i].key, key)) {
			return i;
		}
	}
	/* rotate round */
	for (uint32_t i = 0; i < res; i++) {
		if (tbl[i].key == 0) {
			return i;
		} else if (secukey_equal_p(tbl[i].key, key)) {
			return i;
		}
	}
	/* means we're full :O */
	return -1;
}

/**
 * Return the annotation of a tseries ts from within a tscache object. */
static inline ts_anno_t
tscache_tseries_annotation(tseries_t ts)
{
	return &(((anno_tseries_t)(void*)ts)->anno);
}

void
tscache_bang_anno(tseries_t ts, ts_anno_t anno)
{
	ts_anno_t tgt = tscache_tseries_annotation(ts);
	if (tgt->instr == 0) {
		memcpy(tgt, anno, sizeof(*anno));
		tgt->next = NULL;
	} else {
		ts_anno_t tmp = tgt;
		/* skip to the right tgt */
		for (; tmp->next; tmp = tmp->next);
		tgt = xnew(*tgt);
		tmp->next = tgt;
		memcpy(tgt, anno, sizeof(*anno));
		tgt->next = NULL;
	}
	return;
}

void
tscache_unbang_anno(ts_anno_t anno, tseries_t ts)
{
	memcpy(anno, tscache_tseries_annotation(ts), sizeof(*anno));
	return;
}

static inline tseries_t
tscache_series(_tscache_t c, index_t i)
{
	return c->tbl[i].val;
}

static void
init_tbl(_tscache_t c, size_t ini_sz)
{
	size_t msz = ini_sz * sizeof(*c->tbl);
	c->tbl = malloc(msz);
	memset(c->tbl, 0, msz);
	return;
}

static void
free_tbl(_tscache_t c)
{
	for (index_t i = 0; i < c->alloc_sz; i++) {
		if (secukey_valid_p(c->tbl[i].key)) {
			free_itree(c->tbl[i].val);
		}
	}
	free(c->tbl);
	return;
}

static void
resize_tbl(_tscache_t c, size_t old_sz, size_t new_sz)
{
	size_t new_msz = new_sz * sizeof(*c->tbl);
	struct keyval_s *new = malloc(new_msz);

	memset(new, 0, new_msz);
	for (uint32_t i = 0; i < old_sz; i++) {
		uint32_t new_s;

		if (c->tbl[i].key == 0) {
			continue;
		}
		new_s = slot(new, new_sz, c->tbl[i].key);
		new[new_s].key = c->tbl[i].key;
		new[new_s].val = c->tbl[i].val;
	}
	free(c->tbl);
	/* assign the new one */
	c->tbl = new;
	return;
}


/* ctor, dtor */
tscache_t
make_tscache(void)
{
	_tscache_t res = xnew(struct tscache_s);

	pthread_mutex_init(&res->mtx, NULL);
	init_tbl(res, INITIAL_SIZE);
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

	free_tbl(c);
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
		resize_tbl(c, old_sz, new_sz);
		c->alloc_sz = new_sz;
		/* indicate that we did resize */
		return true;
	}
	/* not resized, all slots should be intact */
	return false;
}


tseries_t
tscache_bang_series(tscache_t tsc, secu_t s, tseries_t ts)
{
/* returns the next instrument we've banged into */
	_tscache_t c = tsc;
	anno_tseries_t res;
	secukey_t sk = secukey_from_secu(s);
	/* key slot and instr slot */
	uint32_t ks, is;

	pthread_mutex_lock(&c->mtx);
	(void)check_resize(c);
	ks = slot(c->tbl, c->alloc_sz, sk);
	if (UNLIKELY(secukey_valid_p(c->tbl[ks].key))) {
		res = NULL; //tscache_tseries(c, c->tbl[ks].val);
		pthread_mutex_unlock(&c->mtx);
		return (void*)res;
	}
	is = c->nseries++;
	c->tbl[ks].key = sk;
	c->tbl[ks].val = make_itree(); //is;
	//res = tscache_anno_tseries(c, is);
	//memcpy(res, ts, sizeof(*ts));
	pthread_mutex_unlock(&c->mtx);
	return (void*)res;
}

tseries_t
find_tseries_by_secu(tscache_t tsc, secu_t secu)
{
	tseries_t res;
	_tscache_t c = tsc;
	uint32_t ks;

	pthread_mutex_lock(&c->mtx);
	ks = slot(c->tbl, c->alloc_sz, secukey_from_secu(secu));
	if (UNLIKELY(ks == -1U)) {
		res = NULL;
	} else if (LIKELY(secukey_valid_p(c->tbl[ks].key))) {
		res = c->tbl[ks].val;
	} else {
		res = NULL;
	}
	pthread_mutex_unlock(&c->mtx);
	return res;
}

/* tseries.c ends here */
