/*** unserding.c -- unserding network service
 *
 * Copyright (C) 2008 Sebastian Freundt
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

#if !defined INCLUDED_unserding_h_
#define INCLUDED_unserding_h_

#include <stdbool.h>

#define UD_NETWORK_SERVICE	"8653"
/* 239.0.0.0/8 are organisational solicited v4 mcast addrs */
#define UD_MCAST4_ADDR		"239.86.53.1"
/* ff3x::8000:0-ff3x::ffff:ffff - dynamically allocated by hosts when needed */
#define UD_MCAST6_ADDR		"ff38:8653::1"

/**
 * Flags. */
typedef long unsigned int ud_flags_t;

/**
 * The catalogue data type. */
typedef void *ud_cat_t;

/** Flag to indicate this is merely a `branch holder'. */
#define UD_CF_JUSTCAT		0x01
/** Flag to indicate a spot value could be obtained (current quote). */
#define UD_CF_SPOTTABLE		0x02
/** Flag to indicate a bid and ask price could be obtained. */
#define UD_CF_TRADABLE		0x04
/** Flag to indicate a last trade can be obtained. */
#define UD_CF_LAST		0x08
/** Flag to indicate */

#define UD_CAT_PREDICATE(_x, _f)					\
	static inline bool __attribute__((always_inline, gnu_inline))	\
	ud_cf_##_x##p(ud_flags_t flags)					\
	{								\
		return flags & (_f);					\
	}								\
	static inline bool __attribute__((always_inline, gnu_inline))	\
	ud_cat_##_x##p(ud_cat_t cat)					\
	{								\
		return ud_cf_##_x##p(((struct ud_cat_s*)cat)->flags);	\
	}


/** Global catalogue. */
extern ud_cat_t ud_catalogue;
/** Instruments. */
extern ud_cat_t instr;
/** Equity instruments. */
extern ud_cat_t equit;
/** Commodity instruments. */
extern ud_cat_t commo;
/** Currency instruments. */
extern ud_cat_t curnc;
/** Interest instruments. */
extern ud_cat_t intrs;
/** Derivatives. */
extern ud_cat_t deriv;

/**
 * The catalogue data structure.
 * Naive. */
struct ud_cat_s {
	ud_cat_t parent;
	ud_cat_t fir_child;
	ud_cat_t las_child;
	ud_cat_t next;
	ud_cat_t prev;
	const void *data;
	ud_flags_t flags;
};

/* predicate magic */
UD_CAT_PREDICATE(justcat, 0x01);
UD_CAT_PREDICATE(spottable, 0x02);
UD_CAT_PREDICATE(tradable, 0x04);
UD_CAT_PREDICATE(last, 0x08);

/* some catalogue functions */
static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_first_child(ud_cat_t cat)
{
	return ((struct ud_cat_s*)cat)->fir_child;
}

static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_last_child(ud_cat_t cat)
{
	return ((struct ud_cat_s*)cat)->las_child;
}

static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_parent(ud_cat_t cat)
{
	if (((struct ud_cat_s*)cat)->parent != NULL) {
		return ((struct ud_cat_s*)cat)->parent;
	} else {
		return ud_catalogue;
	}
}

static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_next_child(ud_cat_t cat)
{
	return ((struct ud_cat_s*)cat)->next;
}

static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_prev_child(ud_cat_t cat)
{
	return ((struct ud_cat_s*)cat)->prev;
}

static inline ud_cat_t __attribute__((always_inline, gnu_inline))
ud_cat_add_child(ud_cat_t cat, const void *data, ud_flags_t flags)
{
	struct ud_cat_s *cell = malloc(sizeof(struct ud_cat_s));
	cell->parent = cat;
	cell->fir_child = cell->las_child = cell->next = NULL;

	if ((cell->prev = ud_cat_last_child(cat)) != NULL) {
		((struct ud_cat_s*)cell->prev)->next = cell;
	} else {
		/* fix up cell's parent */
		((struct ud_cat_s*)cat)->fir_child = cell;
	}
	cell->data = data;
	cell->flags = flags;
	/* fix up the parent of cell */
	((struct ud_cat_s*)cat)->las_child = cell;
	return (ud_cat_t)cell;
}

static inline void __attribute__((always_inline, gnu_inline))
ud_cat_free(ud_cat_t cat)
{
	free(cat);
	return;
}

static inline const void __attribute__((always_inline, gnu_inline)) *
ud_cat_data(const ud_cat_t cat)
{
	return ((struct ud_cat_s*)cat)->data;
}

/* instruments */
extern void init_instr(void);


#endif	/* INCLUDED_unserding_h_ */
