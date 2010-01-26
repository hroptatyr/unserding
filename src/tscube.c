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

typedef struct __bbs_s {
	sl1t_t tgt;
	size_t tsz;
	su_secu_t *sv;
	index_t si;
	/* bit ugly but this slot holds the secu we're talking about */
	su_secu_t s;
	index_t ntk;
	/* for the callback approach */
	tsc_find_f cb;
	void *clo;
} *__bbs_t;


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

static inline __attribute__((unused)) size_t
tsc_box_nticks(tsc_box_t b)
{
	return b->nt;
}

static inline __attribute__((unused)) time32_t
tsc_box_beg(tsc_box_t b)
{
	if (b->beg) {
		return b->beg;
	}
	/* look at the first tick in B */
	return b->beg = scom_thdr_sec(b->sl1t[0].hdr);
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
	return b->end = scom_thdr_sec(b->sl1t[(b->nt - 1) * b->pad].hdr);
#endif	/* CUBE_ENTRY_PER_TTF */
}


/* finders */
static uint16_t
__bbs_find_idx(__bbs_t clo, su_secu_t s)
{
/* behave a bit like the utehdr */
	uint16_t i;

	if (clo->sv == NULL) {
		return 0;
	}
	for (i = 0; i < clo->si; i++) {
		if (clo->sv[i].mux == s.mux) {
			return i;
		}
	}
	if (i < clo->tsz) {
		clo->sv[clo->si++].mux = s.mux;
	}
	return i;
}

static inline bool
ttf_coincide_p(uint16_t tick_ttf, tsc_key_t key)
{
#if defined CUBE_ENTRY_PER_TTF
	return (tick_ttf & 0x0f) == key->ttf;
#else  /* !CUBE_ENTRY_PER_TTF */
	return (1 << (tick_ttf & 0x0f)) & key->ttf || (key->msk & 8) == 0;
#endif	/* CUBE_ENTRY_PER_TTF */
}

static void
tsc_box_find_bbs(__bbs_t clo, tsc_box_t b, tsc_key_t key)
{
	/* to store the last seen ticks of each tick type 0 to 7 */
	const_sl1t_t lst[8] = {0};
	const_sl1t_t lim, t;

	if (b == NULL) {
		return;
	}

	/* sequential scan */
	lim = b->sl1t + b->nt * b->pad;
	for (t = b->sl1t; t < lim && sl1t_stmp_sec(t) <= key->beg; ) {
		uint16_t tkttf = sl1t_ttf(t);
		/* keep track */
		if (ttf_coincide_p(tkttf, key)) {
			lst[(tkttf & 0x0f)] = t;
		}
		t += b->pad;
	}
	for (int i = 0; i < countof(lst) && clo->ntk < clo->tsz; i++) {
		if (lst[i]) {
			sl1t_t tgt = clo->tgt + clo->ntk;
			uint16_t new_idx = __bbs_find_idx(clo, clo->s);
			memcpy(tgt, lst[i], b->pad * sizeof(*b->sl1t));
			scom_thdr_set_tblidx(tgt->hdr, new_idx);
			clo->ntk += b->pad;
		}
	}
	return;
}

static void
tsc_box_find_cb(__bbs_t clo, tsc_box_t b, tsc_key_t key)
{
	/* to store the last seen ticks of each tick type 0 to 7 */
	const_sl1t_t lst[8][2] = {0};
	const_sl1t_t lim, t;

	if (b == NULL) {
		return;
	}
	if (clo->cb == NULL) {
		return;
	}

	/* sequential scan */
	lim = b->sl1t + b->nt * b->pad;
	for (t = b->sl1t; t < lim && sl1t_stmp_sec(t) <= key->beg; ) {
		uint16_t tkttf = sl1t_ttf(t);
		/* keep track */
		if (ttf_coincide_p(tkttf, key)) {
			lst[(tkttf & 0x0f)][1] = lst[(tkttf & 0x0f)][0];
			lst[(tkttf & 0x0f)][0] = t;
		}
		t += b->pad;
	}
	for (int i = 0; i < countof(lst); i++) {
		if (lst[i][1]) {
			clo->cb(clo->s, lst[i][1], clo->clo);
		}
		if (lst[i][0]) {
			clo->cb(clo->s, lst[i][0], clo->clo);
		}
	}
	return;
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
		id1->ttf == id2->ttf
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
#if 0
	} else {
		fprintf(stderr, "%u/%i@%hu already known\n",
			su_secu_quodi(ce->key->secu),
			su_secu_quoti(ce->key->secu),
			su_secu_pot(ce->key->secu));
#endif
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

/* the main problem with the cube is how far we can go back before
 * the timestamp in question, and secondarily how many ticks to leech.
 * in the resulting box we want to be able to find the last tick
 * before the key stamp in question but also want to allow for as
 * many ticks after that for caching purposes.
 * In other words, we're fucked, tick files these days cover different
 * orders of magnitudes of time stamps, from end-of-month data
 * covering many centuries, to high-volume intraday tick data where
 * hundreds of quotes/trades occur within a second.
 * So, the upshot is we must not arrogate to fiddle with the keying
 * timestamp ourselves, instead we should let the underlying data
 * source decide on how far back to go. */
static tsc_box_t
bother_cube(tscube_t utsc, tsc_key_t key, keyval_t kv)
{
/* sig subject to change */
	size_t res = 0;
	tsc_box_t box;

	/* bother the interval trees first */
	assert(kv->intv != NULL);

	if ((box = tsc_itr_find(kv->intv, key->beg)) == NULL &&
	    kv->ce->ops && kv->ce->ops->fetch_cb != NULL) {
		time32_t beg = key->beg;
		/* open end */
		time32_t end = 0x7fffffff;

#if 0
/* not even worth printing in debugging mode */
		size_t mt = tsc_box_maxticks(box);
		fprintf(stderr, "uncached %zu %i %i\n", mt, beg, key->beg);
		itree_print(kv->intv);
#endif

		box = make_tsc_box();
		res = kv->ce->ops->fetch_cb(
			box, tsc_box_maxticks(box),
			kv->ce->key, kv->ce->uval,
			beg, end);

		if (LIKELY(res != 0)) {
			/* we highly encourage for any data source connected
			 * to set these values lest we compute them
			 * ourselves losing some valuable information about
			 * the true nature of the time series obviously */
			beg = tsc_box_beg(box);
			end = tsc_box_end(box);
			/* add the box to our interval tree */
			tsc_itr_add(kv->intv, beg, end, box);

		} else {
			/* what a waste of time */
			free_tsc_box(box);
			box = NULL;
		}
	}
	/* return to who called us, they'll sort the box out */
	return box;
}

size_t
tsc_find1(sl1t_t tgt, size_t tsz, tscube_t tsc, tsc_key_t key)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	struct __bbs_s clo = {
		.tgt = tgt,
		.tsz = tsz,
		.sv = NULL,
		.si = 0,
		.ntk = 0,
	};

	/* filter out stupidity */
	if (UNLIKELY(tsz == 0)) {
		return 0;
	}

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		tsc_ce_t ce = m->tbl[i].ce;
		if (!__key_valid_p(ce->key)) {
			continue;
		} else if (__key_matches_p(ce->key, key)) {
			tsc_box_t box;
			if ((box = bother_cube(tsc, key, &m->tbl[i]))) {
				tsc_box_find_bbs(&clo, box, key);
				/* update the cache-add stamp */
				box->cats = __stamp().tv_sec;
			}
			break;
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return clo.ntk;
}

size_t
tsc_find(sl1t_t tgt, su_secu_t *sv, size_t tsz, tscube_t tsc, tsc_key_t key)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	struct __bbs_s clo = {
		.tgt = tgt,
		.tsz = tsz,
		.sv = sv,
		.si = 0,
		.ntk = 0,
	};

	/* filter out stupidity */
	if (UNLIKELY(tsz == 0)) {
		return 0;
	}

	if (key->secu.mux == 0) {
		fprintf(stderr, "looking for 0-secu\n");
		return 0;
	}

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		tsc_ce_t ce = m->tbl[i].ce;
		if (!__key_valid_p(ce->key)) {
			continue;
		} else if (__key_matches_p(ce->key, key)) {
			tsc_box_t box;
#if 1
			su_secu_t s = ce->key->secu;
			uint32_t qd = su_secu_quodi(s);
			int32_t qt = su_secu_quoti(s);
			uint16_t p = su_secu_pot(s);
			fprintf(stderr, "found match %u/%i@%hu\n", qd, qt, p);
#endif
			if ((box = bother_cube(tsc, key, &m->tbl[i]))) {
				/* tell tsc_box bout our secu */
				clo.s = ce->key->secu;
				tsc_box_find_bbs(&clo, box, key);
				/* update the cache-add stamp */
				box->cats = __stamp().tv_sec;
			}
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return clo.ntk;
}

void
tsc_find_cb(tscube_t tsc, tsc_key_t key, tsc_find_f cb, void *clo)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;
	struct __bbs_s bbclo = {
		.si = 0,
		.ntk = 0,
		.cb = cb,
		.clo = clo,
	};

	fprintf(stderr, "%p %p\n", cb, clo);
	if (key->secu.mux == 0) {
		fprintf(stderr, "looking for 0-secu\n");
		return;
	}

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		tsc_ce_t ce = m->tbl[i].ce;
		if (!__key_valid_p(ce->key)) {
			continue;
		} else if (__key_matches_p(ce->key, key)) {
			tsc_box_t box;

			if ((box = bother_cube(tsc, key, &m->tbl[i]))) {
				/* tell tsc_box bout our secu */
				bbclo.s = ce->key->secu;
				tsc_box_find_cb(&bbclo, box, key);
				/* update the cache-add stamp */
				box->cats = __stamp().tv_sec;
			}
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return;
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


/* administrative bollocks */
static time32_t
box_age(tsc_box_t b)
{
	return __stamp().tv_sec - b->cats;
}

static void
list_box(it_node_t nd, void *UNUSED(clo))
{
	time32_t age = box_age(nd->data);

	fprintf(stderr, "%i %i  age %i\n", nd->lo, nd->hi, age);
	return;
}

void
tsc_list_boxes(tscube_t tsc)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;

	pthread_mutex_lock(&m->mtx);
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		tsc_ce_t ce = m->tbl[i].ce;
		if (__key_valid_p(ce->key)) {
			itree_trav_in_order(m->tbl[i].intv, list_box, NULL);
		}
	}
	pthread_mutex_unlock(&m->mtx);
	return;
}

/* cache pruning */
#define PRUNE_INPLACE	1

#if !defined PRUNE_INPLACE
static size_t prunn;
static struct {
	it_node_t n;
	tsc_itr_t t;
} prunv[16];
#endif	/* !PRUNE_INPLACE */

static void
prune_box(it_node_t nd, void *clo)
{
	tsc_itr_t tr = clo;
	tsc_box_t b = nd->data;

	if (box_age(b) >= TSC_BOX_TTL) {
		fprintf(stderr, "rem'ing box %p\n", b);
#if defined PRUNE_INPLACE
		b = itree_del_node_nl(tr, nd);
		free_tsc_box(b);
		fprintf(stderr, "rem'd box %p\n", b);
#else  /* !PRUNE_INPLACE */
		prunv[prunn].n = nd;
		prunv[prunn].t = tr;
		prunn++;
#endif	/* PRUNE_INPLACE */
	} else {
		fprintf(stderr, "box %p not ripe\n", b);
	}
	return;
}

void
tsc_prune_caches(tscube_t tsc)
{
	__tscube_t c = tsc;
	hmap_t m = c->hmap;

	pthread_mutex_lock(&m->mtx);
#if !defined PRUNE_INPLACE
	prunn = 0;
#endif	/* !PRUNE_INPLACE */
	/* perform sequential scan */
	for (uint32_t i = 0; i < m->alloc_sz; i++) {
		tsc_ce_t ce = m->tbl[i].ce;
		if (__key_valid_p(ce->key)) {
			tsc_itr_t tr = m->tbl[i].intv;
			itree_trav_in_order(tr, prune_box, tr);
		}
	}
	pthread_mutex_unlock(&m->mtx);
#if !defined PRUNE_INPLACE
	for (int i = 0; i < prunn; i++) {
		tsc_box_t b = itree_del_node(prunv[i].t, prunv[i].n);
		fprintf(stderr, "rem'd box %p\n", b);
		free_tsc_box(b);
	}
#endif	/* !PRUNE_INPLACE */
	return;
}

/* tscube.c ends here */
