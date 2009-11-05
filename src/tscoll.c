/*** tscoll.c -- collections of time series
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
#include <time.h>
#include <pfack/uterus.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"
#include "urn.h"
#include "tscoll.h"
#include "intvtree.h"
#include "unserding-nifty.h"

tscoll_t
make_tscoll(secu_t s)
{
	return make_itree_sat(s, sizeof(*s));
}

void
free_tscoll(tscoll_t tsc)
{
	free_itree(tsc);
	return;
}

secu_t
tscoll_secu(tscoll_t tsc)
{
	return itree_satellite(tsc);
}

void
tscoll_add(tscoll_t tsc, tseries_t sp)
{
	tseries_t copy_sp = xnew(*sp);
	time_t from = sp->from;
	time_t to = sp->to;

	memcpy(copy_sp, sp, sizeof(*sp));
	copy_sp->secu = tscoll_secu(tsc);
	copy_sp->private = make_itree();
	itree_add(tsc, from, to, copy_sp);
	return;
}

tseries_t
tscoll_add2(tscoll_t tsc, const_urn_t urn, time_t from, time_t to, uint32_t ty)
{
	tseries_t sp = xnew(*sp);

	sp->urn = urn;
	sp->from = from;
	sp->to = to;
	sp->types = ty;
	sp->secu = tscoll_secu(tsc);
	sp->private = make_itree();
	itree_add(tsc, from, to, sp);
	return sp;
}

tseries_t
tscoll_find_series(tscoll_t tsc, time_t ts)
{
	return itree_find_point(tsc, ts);
}

tser_pkt_t
tseries_find_pkt(tseries_t tser, time_t ts)
{
	return itree_find_point(tser->private, ts);
}

void
tseries_add(tseries_t tser, dse16_t beg, dse16_t end, tser_pkt_t pkt)
{
	tser_pkt_t p = xnew(*p);
	memcpy(p, pkt, sizeof(*p));
	itree_add(tser->private, beg, end, p);
	return;
}

/* iterators */
void
tscoll_trav_series(tscoll_t tsc, tscoll_trav_f cb, void *clo)
{
	itree_trav_in_order(tsc, cb, clo);
	return;
}

/* tscoll.c ends here */
