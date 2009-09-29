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

/**
 * Like tseries but with annotation goodness. */
typedef struct anno_tseries_s *anno_tseries_t;
struct anno_tseries_s {
	struct tseries_s tseries;
	struct ts_anno_s anno;
};


/* helpers */

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

#if 0

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
	c->nseries++;
	c->tbl[ks].key = sk;
	res = c->tbl[ks].val = make_itree();
	pthread_mutex_unlock(&c->mtx);
	return (void*)res;
}
#endif

/* tseries.c ends here */
