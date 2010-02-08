/*** tscube.h -- time series cube
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

#if !defined INCLUDED_tscube_h_
#define INCLUDED_tscube_h_

#include <stdint.h>
#include <sushi/secu.h>
#include <sushi/scommon.h>
#include <sushi/sl1t.h>
#include <sushi/scdl.h>
#include "ud-time.h"

/* tuning */
#undef CUBE_ENTRY_PER_TTF

typedef void *tscube_t;

/** Cube entry keys. */
typedef struct tsc_key_s *tsc_key_t;
/** Cube entries. */
typedef struct tsc_ce_s *tsc_ce_t;
/* cube operations (or methods if you will) */
typedef struct tsc_ops_s *tsc_ops_t;

typedef struct tsc_box_s *tsc_box_t;

/**
 * The idea of the fetch function is to fetch (a subseries of) the time
 * series specified by K (from BEG till END).
 * Callbacks must fill in TGT correctly, i.e. the number of ticks, the
 * beginning and the end time stamps. */
typedef size_t(*fetch_f)(
	tsc_box_t tgt, size_t tsz, tsc_key_t k, void *uval,
	time32_t beg, time32_t end);

/**
 * The idea of the urn refetch function is to allow for ... */
typedef void(*urn_refetch_f)(tsc_key_t k, void *uval);

/**
 * For cube traversals, generic callback function. */
typedef void(*tsc_trav_f)(tsc_ce_t ce, void *clo);

/**
 * Keys for cube entries that can be searched for. */
struct tsc_key_s {
	time32_t beg, end;
	su_secu_t secu;
	uint16_t ttf;
	/* two times 64 bit + 16, leaves 48 bits of general purpose */
	/** when used as search input this masks the fields that are set */
	uint16_t msk;
	/** general purpose 32bit integer, used as index mostly */
	uint32_t gen;
};

struct tsc_ce_s {
	struct tsc_key_s key[1];

	/**
	 * Following is not directly searchable, but yields different
	 * cube entries upon different values */
	tsc_ops_t ops;
	/** General purpose user value. */
	void *uval;
};

struct tsc_ops_s {
	/** callback fun to call when ticks are to be fetched */
	fetch_f fetch_cb;
	urn_refetch_f urn_refetch_cb;
};

/* desired size of the box structure */
#define TSC_BOX_SZ	4096

struct tsc_box_s {
	/* keep track how large this is */
	uint16_t nt, skip;
	/* time stamp when we added the box (cache-add time-stamp) */
	time32_t cats;

	/* offset 0x08 */
	/* also keep track which ticks are supposed to be in here */
	time32_t beg, end;

	/* offset 0x10 */
	uint64_t secu[2];

	/* offset 0x20, 4 * 64 = 2 * 4 * 32 = 2 sl1t = 1 scdl */
#if 0
/* grrr, fucking gcc chokes on this */
	union {
		struct sl1t_s sl1t[];
		struct scdl_s scdl[];
	};
#else
	struct sl1t_s sl1t[] __attribute__((aligned(sizeof(struct scdl_s))));
#endif
};


/* tscube funs */
extern tscube_t make_tscube(void);
extern void free_tscube(tscube_t);
extern size_t tscube_size(tscube_t);

extern tsc_ce_t tsc_get(tscube_t c, tsc_key_t key);
extern void tsc_add(tscube_t c, tsc_ce_t ce);

/* quick hack, find the first entry in tsc that matches key */
extern size_t tsc_find1(sl1t_t tgt, size_t tsz, tscube_t c, tsc_key_t key);

/* quick hack, find all entries in tsc that matches key,
 * sv should have TSZ entries */
extern size_t
tsc_find(sl1t_t tgt, su_secu_t *sv, size_t tsz, tscube_t c, tsc_key_t key);

/* nother quick hack, used to traverse over all cube entries in TSC based
 * on a query KEY, calling CB with the ce and CLO for each match. */
extern void tsc_trav(tscube_t tsc, tsc_key_t key, tsc_trav_f cb, void *clo);

/* administrative stuff */
extern void tsc_list_boxes(tscube_t tsc);
extern void tsc_prune_caches(tscube_t tsc);

#define TSC_BOX_TTL	180

typedef void(*tsc_find_f)(tsc_box_t, su_secu_t, void*);
extern void
tsc_find_cb(tscube_t tsc, tsc_key_t key, tsc_find_f cb, void *clo);

#endif	/* !INCLUDED_tscube_h_ */
