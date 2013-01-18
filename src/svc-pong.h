/*** svc-pong.h -- pong service goodies
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#if !defined INCLUDED_svc_pong_h_
#define INCLUDED_svc_pong_h_

#include <stdbool.h>
#include <stdint.h>

/**
 * Bitset of server scores.  This is a generalisation of the
 * master/slave concept. */
typedef uint32_t ud_pong_set_t;

/**
 * Type for server scores. */
typedef uint8_t ud_pong_score_t;

/**
 * Maximum number of concurrent servers on the network.
 * We use 32 so we can keep track of server scores in a uint32_t value. */
#define UD_MAX_CONCUR	(sizeof(ud_pong_set_t) * 8)
#define UD_LOW_SCORE	((ud_pong_set_t)(UD_MAX_CONCUR - 1))

static inline ud_pong_set_t
ud_empty_pong_set(void)
{
	return (uint32_t)0;
}

static inline ud_pong_set_t
ud_pong_set(ud_pong_set_t ps, ud_pong_score_t s)
{
	return ps | (1 << (s & UD_LOW_SCORE));
}

static inline ud_pong_set_t
ud_pong_unset(ud_pong_set_t ps, ud_pong_score_t s)
{
	return ps & ~(1 << (s & UD_LOW_SCORE));
}

static inline uint8_t
__nright_zeroes(uint32_t x)
{
/* reiser's method, map a bit value mod 37 to its position
 * returns the bit-position of the first 1, or alternatively speaking
 * the number of zero bits on the right */
	static const uint8_t tbl[] = {
		32, 0, 1, 26, 2, 23, 27, 0, 3,
		16, 24, 30, 28, 11, 0, 13, 4,
		7, 17, 0, 25, 22, 31, 15, 29,
		10, 12, 6, 0, 21, 14, 9, 5,
		20, 8, 19, 18
	};
	return tbl[(-x & x) % 37];
}

static inline ud_pong_score_t
ud_find_score(ud_pong_set_t ps)
{
/* find ourselves a score that would suit us */
	return __nright_zeroes(~ps);
}


#if defined UD_COMPAT
/* exports, will be in libunserding */
extern ud_pong_score_t
ud_svc_nego_score(ud_handle_t hdl, int timeout);
#endif	/* UD_COMPAT */

#endif	/* INCLUDED_svc_pong_h_ */
