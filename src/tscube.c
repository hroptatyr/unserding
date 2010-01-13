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
/* tscube has no hashing at the mo, means it's just an ordinary array-based
 * sequentially scanned list */

#if !defined _BSD_SOURCE
# define _BSD_SOURCE
#endif	/* !_BSD_SOURCE */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

#include <sushi/secu.h>
#include <sushi/scommon.h>
#include "tscube.h"
#include "unserding-nifty.h"

#include <stdio.h>
#if defined USE_ASSERTIONS
# include <assert.h>
#else
# define assert(args...)
#endif	/* USE_ASSERTIONS */


/* decouple from intvtree madness */
#include "intvtree.h"

typedef itree_t tsc_itr_t;

#define make_tsc_itr	make_itree
#define free_tsc_itr	free_itree
#define tsc_itr_add	itree_add
#define tsc_itr_find	itree_find_point


#define INITIAL_SIZE	1024

typedef struct tscube_s *__tscube_t;
/* helpers */
typedef struct hmap_s *hmap_t;

typedef struct keyval_s *keyval_t;

/**
 * Just a reverse lookup structure. */
struct keyval_s {
	/** user provided key */
	struct tsc_ce_s ce[1];
	/** interval trees to see what's cached: ce + (b, e) |-> sl1t */
	tsc_itr_t intv;
};

/**
 * The master hash table for the cube which maps secu + ttf to itrees.
 * Naive, just an array of time series interval trees, used as hash table
 * per the keys vector which gives a primitive lookup from
 * the hashing key to the index in the former vector. */
struct hmap_s {
	size_t nseries;
	size_t alloc_sz;
	struct keyval_s *tbl;
	pthread_mutex_t mtx;
};

/**
 * The time series cube data structure. */
struct tscube_s {
	struct hmap_s hmap[1];
	/** global itree to answer questions like, which instruments did
	 * exist given a time stamp or time range. */
};


/* for one bunch of ticks */
/* we'll just use the whole allocation for ticks */
#define TSC_BOX_SZ	4096

#define MAP_MEMMAP	(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEMMAP	(PROT_READ | PROT_WRITE)
static tsc_box_t
make_tsc_box(void)
{
	return mmap(NULL, TSC_BOX_SZ, PROT_MEMMAP, MAP_MEMMAP, 0, 0);
}

static void
free_tsc_box(tsc_box_t b)
{
	munmap(b, TSC_BOX_SZ);
	return;
}

static inline size_t
tsc_box_maxticks(tsc_box_t b)
{
	/* either 254 ticks or 127 candles */
	return TSC_BOX_SZ / sizeof(*b->sl1t) - (b->sl1t - (sl1t_t)b);
}

static inline size_t
tsc_box_nticks(tsc_box_t b)
{
	return b->nt;
}

static inline __attribute__((unused)) time32_t
tsc_box_beg(tsc_box_t b)
{
	return b->beg;
}

static inline time32_t
tsc_box_end(tsc_box_t b)
{
	if (b->end) {
		return b->end;
	}
	/* grrr, look at the last index */
#if defined CUBE_ENTRY_PER_TTF
	switch (b->pad) {
	case 1:
		/* sl1t */
		return b->end = sl1t_stmp_sec(&b->sl1t[b->nt - 1]);
	case 2:
		/* scdl */
		return b->end = scdl_stmp_sec(&b->scdl[b->nt - 1]);
	default:
		/* bullshit */
		return 0;
	}
#else  /* !CUBE_ENTRY_PER_TTF */
	return b->end = sl1t_stmp_sec(&b->sl1t[(b->nt - 1) * b->pad]);
#endif	/* CUBE_ENTRY_PER_TTF */
}

static sl1t_t
tsc_box_find_bb(tsc_box_t b, time32_t ts)
{
	sl1t_t ar = b->sl1t;
	int fiddle = b->pad;
	index_t i, hi = tsc_box_nticks(b) * fiddle, lo = 0;

	do {
		i = (hi + lo) / 2 & -fiddle;
		//fprintf(stderr, "%zu %zu %zu %u\n", i, lo, hi, sl1t_stmp_sec(ar + i));
		if (sl1t_stmp_sec(ar + i) <= ts &&
		    sl1t_stmp_sec(ar + i + fiddle) > ts) {
			return ar + i;
		} else if (hi == lo) {
			return NULL;
		} else if (sl1t_stmp_sec(ar + i) > ts) {
			/* prefer lower half */
			hi = i - fiddle;
		} else {
			/* strictly less, prefer upper half */
			lo = i + fiddle;
		}
	} while (true);
	/* not reached */
}


/* helpers, for hmap */
static inline bool
__key_equal_p(tsc_key_t id1, tsc_key_t id2)
{
	return id1 && id2 &&
		id1->beg == id2->beg &&
		id1->end == id2->end &&
		id1->secu.mux == id2->secu.mux &&
#if defined CUBE_ENTRY_PER_TTF
/* try ttf-less approach */
		id1->ttf == id2->ttf
#else  /* !CUBE_ENTRY_PER_TTF */
		true
#endif	/* CUBE_ENTRY_PER_TTF */
		;
}

static inline bool
__key_valid_p(tsc_key_t id1)
{
	return su_secu_ui64(id1->secu) != 0;
}

static void
init_tbl(hmap_t c, size_t ini_sz)
{
	size_t msz = ini_sz * sizeof(*c->tbl);
	c->tbl = xmalloc(msz);
	memset(c->tbl, 0, msz);
	return;
}

static void
free_tbl(hmap_t c)
{
	for (index_t i = 0; i < c->alloc_sz; i++) {
		if (__key_valid_p(c->tbl[i].ce->key)) {
#if 0
			if (c->tbl[i].val->intv) {
				free_tsc_itr(c->tbl[i].val->intv);
			}
#endif
		}
	}
	xfree(c->tbl);
	return;
}

#define SLOTS_FULL	((uint32_t)0xffffffff)
static uint32_t
slot(struct keyval_s *tbl, size_t size, tsc_key_t key)
{
/* return the first slot in c->tbl that either contains gaid or would
 * be a warm n cuddly place for it
 * linear probing */
	/* normally we obtain a good starting value here */
	uint32_t res = 0;
	for (uint32_t i = res; i < size; i++) {
		if (!__key_valid_p(tbl[i].ce->key)) {
			return i;
		} else if (__key_equal_p(tbl[i].ce->key, key)) {
			return i;
		}
	}
	/* rotate round */
	for (uint32_t i = 0; i < res; i++) {
		if (!__key_valid_p(tbl[i].ce->key)) {
			return i;
		} else if (__key_equal_p(tbl[i].ce->key, key)) {
			return i;
		}
	}
	/* means we're full :O */
	return SLOTS_FULL;
}

static void
resize_tbl(hmap_t c, size_t old_sz, size_t new_sz)
{
	size_t new_msz = new_sz * sizeof(*c->tbl);
	struct keyval_s *new = malloc(new_msz);

	memset(new, 0, new_msz);
	for (uint32_t i = 0; i < old_sz; i++) {
		uint32_t new_s;

		if (!__key_valid_p(c->tbl[i].ce->key)) {
			continue;
		}
		new_s = slot(new, new_sz, c->tbl[i].ce->key);
		new[new_s] = c->tbl[i];
	}
	xfree(c->tbl);
	/* assign the new one */
	c->tbl = new;
	return;
}

static inline bool
check_resize(hmap_t c)
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

static void
init_hmap(hmap_t m)
{
	pthread_mutex_init(&m->mtx, NULL);
	init_tbl(m, INITIAL_SIZE);
	m->alloc_sz = INITIAL_SIZE;
	m->nseries = 0;
	return;
}

static void
free_hmap(hmap_t m)
{
	pthread_mutex_lock(&m->mtx);
	pthread_mutex_unlock(&m->mtx);
	pthread_mutex_destroy(&m->mtx);

	free_tbl(m);
	return;
}


/* ctor, dtor */
tscube_t
make_tscube(void)
{
	__tscube_t res = xnew(*res);

	init_hmap(res->hmap);
	return (void*)res;
}

void
free_tscube(tscube_t tsc)
{
	struct tscube_s *c = tsc;

	free_hmap(c->hmap);
	/* free the cube itself */
	xfree(tsc);
	return;
}

size_t
tscube_size(tscube_t tsc)
{
	__tscube_t c = tsc;
	return c->hmap->nseries;
}

tsc_ce_t
tsc_get(tscube_t tsc, tsc_key_t key)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	uint32_t ks;
	tsc_ce_t res = NULL;

	pthread_mutex_lock(&m->mtx);
	ks = slot(m->tbl, m->alloc_sz, key);
	if (LIKELY(ks != SLOTS_FULL && __key_valid_p(m->tbl[ks].ce->key))) {
		res = m->tbl[ks].ce;
	}
	pthread_mutex_unlock(&m->mtx);
	return res;
}

void
tsc_add(tscube_t tsc, tsc_ce_t ce)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	uint32_t ks;

	pthread_mutex_lock(&m->mtx);
	ks = slot(m->tbl, m->alloc_sz, ce->key);
	if (UNLIKELY(ks == SLOTS_FULL || !__key_valid_p(m->tbl[ks].ce->key))) {
		/* means the slot is there, but it hasnt got a coll
		 * associated */
		(void)check_resize(m);
		ks = slot(m->tbl, m->alloc_sz, ce->key);
		/* oh we want to keep track of this */
		memcpy(m->tbl[ks].ce->key, ce, sizeof(*ce));
		/* create an interval tree */
		m->tbl[ks].intv = make_tsc_itr();
		/* assign user's idea of this */
		m->nseries++;
	}
	pthread_mutex_unlock(&m->mtx);
	return;
}

/* finder */
static bool
__key_matches_p(tsc_key_t matchee, tsc_key_t matcher)
{
	if ((matcher->msk & 1) == 0 && (matcher->end & 2) == 0) {
		/* do nothing, just means we're not filtering by ts */
		;
	} else if (!(matchee->beg <= matcher->beg &&
		     matchee->end >= matcher->end)) {
		/* our series does not overlap matcher entirely */
		return false;
	}

	if ((matcher->msk & 8) == 0) {
		/* bingo, user doesnt care */
		;
#if defined CUBE_ENTRY_PER_TTF
	} else if (matchee->ttf != matcher->ttf) {
		/* ttf's do not match */
		return false;
#else  /* !CUBE_ENTRY_PER_TTF */
	} else if ((matchee->ttf & matcher->ttf) == 0) {
		/* ttf's do not match */
		return false;
#endif	/* CUBE_ENTRY_PER_TTF */
	}

	if ((matcher->msk & 4) == 0) {
		/* user not interested in specifying the secu */
		;
	} else {
		uint32_t qd = su_secu_quodi(matcher->secu);
		int32_t qt = su_secu_quoti(matcher->secu);
		uint16_t p = su_secu_pot(matcher->secu);

		if (qd && qd != su_secu_quodi(matchee->secu)) {
			return false;
		} else if (qt && qt != su_secu_quoti(matchee->secu)) {
			return false;
		} else if (p && p != su_secu_pot(matchee->secu)) {
			return false;
		}
	}
	return true;
}

static __attribute__((unused)) time32_t
last_monday_midnight(time32_t ts)
{
	dow_t dow = __dayofweek(ts);
	time32_t sub = ts % 86400;
	switch (dow) {
	case DOW_SUNDAY:
		sub += 6 * 86400;
		break;
	case DOW_MONDAY:
		sub += 7 * 86400;
		break;
	case DOW_TUESDAY:
		sub += 1 * 86400;
		break;
	case DOW_WEDNESDAY:
		sub += 2 * 86400;
		break;
	case DOW_THURSDAY:
		sub += 3 * 86400;
		break;
	case DOW_FRIDAY:
		sub += 4 * 86400;
		break;
	case DOW_SATURDAY:
		sub += 5 * 86400;
		break;
	case DOW_MIRACLEDAY:
	default:
		break;
	}
	return ts - sub;
}

static __attribute__((unused)) time32_t
today_midnight(time32_t ts)
{
	return ts - (ts % 86400);
}

static __attribute__((unused)) time32_t
today_latenight(time32_t ts)
{
	return today_midnight(ts) + 86399;
}

#define utsc		UNUSED(tsc)
#define utsz		UNUSED(tsz)

static size_t
bother_cube(sl1t_t tgt, size_t utsz, tscube_t utsc, tsc_key_t key, keyval_t kv)
{
/* sig subject to change */
	size_t res = 0;
	tsc_box_t box;
	sl1t_t bb;

	/* bother the interval trees first */
	assert(kv->intv != NULL);

	if ((box = tsc_itr_find(kv->intv, key->beg)) == NULL &&
	    kv->ce->ops && kv->ce->ops->fetch_cb != NULL) {
		time32_t beg = today_midnight(key->beg);
		//time32_t beg = todays_latenight(key->beg);
		//time32_t beg = last_monday_midnight(key->beg);
		/* open end */
		time32_t end = 0x7fffffff;

		size_t mt = tsc_box_maxticks(box);
		fprintf(stderr, "uncached %zu\n", mt);
		box = make_tsc_box();
		res = kv->ce->ops->fetch_cb(
			box, tsc_box_maxticks(box),
			kv->ce->key, kv->ce->uval,
			/* check if it's eligible to actually go back to
			 * the monday before!!! */
			beg, end);

		if (LIKELY(res != 0)) {
			end = tsc_box_end(box);
			/* add the box to our interval tree */
			tsc_itr_add(kv->intv, beg, end, box);

		} else {
			/* what a waste of time */
			free_tsc_box(box);
			box = NULL;
		}
	}

	if (UNLIKELY(box == NULL ||
		     (bb = tsc_box_find_bb(box, key->beg)) == NULL)) {
		fprintf(stderr, "no way of fetching %i\n", key->beg);
		return 0;
	}

	fprintf(stderr, "cached %p\n", box);
	res = box->pad;
	memcpy(tgt, bb, sizeof(*tgt) * res);
	return res;
}

size_t
tsc_find1(sl1t_t tgt, size_t tsz, tscube_t tsc, tsc_key_t key)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	size_t ntk = 0;

	/* filter out stupidity */
	if (UNLIKELY(tsz == 0)) {
		return 0;
	}

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		if (!__key_valid_p(m->tbl[i].ce->key)) {
			continue;
		} else if (__key_matches_p(m->tbl[i].ce->key, key)) {
			ntk = bother_cube(tgt, tsz, tsc, key, &m->tbl[i]);
			fprintf(stderr, "bothered cube, yielded %zu\n", ntk);
			break;
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return ntk;
}

void
tsc_trav(tscube_t tsc, tsc_key_t key, tsc_trav_f cb, void *clo)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		if (__key_valid_p(m->tbl[i].ce->key) &&
		    __key_matches_p(m->tbl[i].ce->key, key)) {
			cb(m->tbl[i].ce, clo);
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return;
}

/* tscube.c ends here */
