/*** tscoll.h -- collections of time series
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

#if !defined INCLUDED_tscoll_h_
#define INCLUDED_tscoll_h_

#include <stdbool.h>
#include <time.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"
#include "urn.h"
#include "tseries.h"

/* tick services */
#if !defined index_t
# define index_t	size_t
#endif	/* !index_t */

typedef void *tscoll_t;
typedef struct tseries_s *tseries_t;
/* for traversal */
typedef void(*tscoll_trav_f)(uint32_t lo, uint32_t hi, void *data, void *clo);

struct tseries_s {
	const_urn_t urn;
	time_t from, to;
	uint32_t types;
	/** points back into the tscoll satellite */
	secu_t secu;
	/** do not fiddle with me */
	void *private;
};

extern tscoll_t make_tscoll(secu_t secu);
extern void free_tscoll(tscoll_t tsc);

extern void tscoll_add(tscoll_t tsc, tseries_t);
extern tseries_t tscoll_add2(tscoll_t, const_urn_t, time_t, time_t, uint32_t);
extern secu_t tscoll_secu(tscoll_t tsc);

extern tseries_t tscoll_find_series(tscoll_t tsc, time_t ts);
/* move to tseries.[ch]? */
extern tser_pkt_t tseries_find_pkt(tseries_t tsc, time_t ts);
extern void tseries_add(tseries_t tsc, dse16_t beg, dse16_t end, tser_pkt_t p);

extern void tscoll_trav_series(tscoll_t tsc, tscoll_trav_f cb, void *clo);

#endif	/* INCLUDED_tscoll_h_ */
