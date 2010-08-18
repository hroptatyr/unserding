/*** unserding-ctx.h -- unserding context
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

#if !defined INCLUDED_unserding_ctx_h_
#define INCLUDED_unserding_ctx_h_

#include "unserding.h"

/**
 * Unserding context structure, passed along to submods. */
typedef struct ud_ctx_s *ud_ctx_t;

/**
 * Opaque data type for settings tables whither configuration goes. */
typedef void *ud_cfgset_t;

/**
 * Guts of the unserding context struct. */
struct ud_ctx_s {
	/** libev's mainloop */
	void *mainloop;
	void *cfgctx;
	ud_cfgset_t curr_cfgset;
	ud_handle_t hdl;
};

/* only used during the module load stage, could be a separate arg one day */
static inline void
udctx_set_setting(ud_ctx_t ctx, ud_cfgset_t setting)
{
	ctx->curr_cfgset = setting;
	return;
}

static inline ud_cfgset_t
udctx_get_setting(ud_ctx_t ctx)
{
	void *res = ctx->curr_cfgset;
	return res;
}

#endif	/* INCLUDED_unserding_ctx_h_ */
