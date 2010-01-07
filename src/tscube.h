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

#if !defined time32_t
typedef int32_t time32_t;
#define time32_t	time32_t
#endif	/* !time32_t */
typedef void *tscube_t;

typedef struct tsc_key_s *tsc_key_t;

struct tsc_key_s {
	time32_t beg, end;
	su_secu_t secu;
	uint16_t ttf;

	/** callback fun to call when ticks are to be fetched */
	size_t(*fetch_cb)(void **, tsc_key_t key, void *val);
};


/* tscube funs */
extern tscube_t make_tscube(void);
extern void free_tscube(tscube_t);
extern size_t tscube_size(tscube_t);

extern void *tsc_get(tscube_t c, tsc_key_t key);
extern void tsc_add(tscube_t c, tsc_key_t key, void *val);

/* quick hack, find the first entry in tsc that matches key */
extern void tsc_find1(tscube_t c, tsc_key_t *key, void **val);

#endif	/* !INCLUDED_tscube_h_ */
