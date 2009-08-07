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


/* ctor, dtor */
cat_t
make_cat(void)
{
	_cat_t res = xnew(struct cat_s);

	pthread_mutex_init(&res->mtx, NULL);
	res->instrs = malloc(sizeof(struct instr_s) * INITIAL_SIZE);
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
	free(c);
	return;
}

static inline instr_t
cat_instr(cat_t cat, index_t i)
{
	_cat_t c = cat;
	instr_t instrs = c->instrs;

	return &instrs[i];
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

static inline void
check_resize(_cat_t c)
{
	if (UNLIKELY(c->ninstrs == c->alloc_sz)) {
		/* resize */
		size_t new_sz = (c->alloc_sz *= 2) * sizeof(struct instr_s);
		c->instrs = realloc(c->instrs, new_sz);
	}
	return;
}

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
/* returns the next available instrument */
	_cat_t c = cat;
	instr_t res;

	pthread_mutex_lock(&c->mtx);
	check_resize(c);
	res = cat_instr(c, c->ninstrs++);
	memcpy(res, i, sizeof(*i));
	pthread_mutex_unlock(&c->mtx);
	return res;
}

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

instr_t
find_instr_by_gaid(cat_t cat, gaid_t gaid)
{
	_cat_t c = cat;

	pthread_mutex_lock(&c->mtx);
	for (index_t i = 0; i < c->ninstrs; i++) {
		instr_t this_instr = cat_instr(c, i);
		ident_t this_ident = instr_ident(this_instr);
		if (gaid_equal_p(ident_gaid(this_ident), gaid)) {
			pthread_mutex_unlock(&c->mtx);
			return this_instr;
		}
	}
	pthread_mutex_unlock(&c->mtx);
	return NULL;
}

instr_t
find_instr_by_name(cat_t cat, const char *name)
{
	_cat_t c = cat;

	pthread_mutex_lock(&c->mtx);
	for (index_t i = 0; i < c->ninstrs; i++) {
		instr_t this_instr = cat_instr(c, i);
		ident_t this_ident = instr_ident(this_instr);
		if (ident_name(this_ident)[0] != '\0' &&
		    name_equal_p(ident_name(this_ident), name)) {
			pthread_mutex_unlock(&c->mtx);
			return this_instr;
		}
	}
	pthread_mutex_unlock(&c->mtx);
	return NULL;
}

/* catalogue-ng.c ends here */
