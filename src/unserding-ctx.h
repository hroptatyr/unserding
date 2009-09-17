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

#undef	USE_LIBCONFIG
#define USE_LUA
#include "unserding.h"
#if defined USE_LIBCONFIG
# include <libconfig.h>
#endif	/* USE_LIBCONFIG */
#if defined USE_LUA
# include "lua-config.h"
#endif	/* USE_LUA */

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
#if defined USE_LIBCONFIG
	/** libconfig's context */
	struct config_t cfgctx;
#elif defined USE_LUA
	void *cfgctx;
#endif	/* USE_LIBCONFIG */
	ud_cfgset_t curr_cfgset;
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

/* config mumbojumbo, just redirs to the lua cruft */
#if defined USE_LUA
static inline ud_cfgset_t
udcfg_tbl_lookup(ud_ctx_t ctx, ud_cfgset_t s, const char *name)
{
	return lc_cfgtbl_lookup(ctx->cfgctx, s, name);
}

static inline void
udcfg_tbl_free(ud_ctx_t ctx, ud_cfgset_t s)
{
	lc_cfgtbl_free(ctx->cfgctx, s);
	return;
}

static inline size_t
udcfg_tbl_lookup_s(const char **t, ud_ctx_t c, ud_cfgset_t s, const char *n)
{
	return lc_cfgtbl_lookup_s(t, c->cfgctx, s, n);
}

static inline size_t
udcfg_glob_lookup_s(const char **t, ud_ctx_t c, const char *n)
{
	return lc_globcfg_lookup_s(t, c->cfgctx, n);
}

static inline bool
udcfg_glob_lookup_b(ud_ctx_t ctx, const char *name)
{
	return lc_globcfg_lookup_b(ctx->cfgctx, name);
}
#endif	/* USE_LUA */

#endif	/* INCLUDED_unserding_ctx_h_ */
