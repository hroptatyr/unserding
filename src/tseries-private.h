/*** tseries.h -- stuff that is soon to be replaced by ffff's tseries
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

#if !defined INCLUDED_tseries_private_h_
#define INCLUDED_tseries_private_h_

#include "tscube.h"

extern tscube_t gcube;


/* module like helpers */
extern void dso_tseries_LTX_init(void*);
extern void dso_tseries_LTX_deinit(void*);
extern void dso_tseries_mysql_LTX_init(void*);
extern void dso_tseries_mysql_LTX_deinit(void*);
extern void dso_tseries_frobq_LTX_init(void*);
extern void dso_tseries_frobq_LTX_deinit(void*);
extern void dso_tseries_sl1t_LTX_init(void*);
extern void dso_tseries_sl1t_LTX_deinit(void*);
extern void dso_tseries_ute_LTX_init(void*);
extern void dso_tseries_ute_LTX_deinit(void*);


/* frob queue mumbo jumbo */
extern void frobnicate(void);


/* hooks */
typedef struct hook_s *hook_t;
typedef void(*hook_f)(job_t);

struct hook_s {
	size_t nf;
	hook_f f[16];
};

extern hook_t fetch_urn_hook;

static inline void
add_hook(hook_t h, hook_f f)
{
	h->f[h->nf++] = f;
	return;
}

#endif	/* INCLUDED_tseries_private_h_ */
