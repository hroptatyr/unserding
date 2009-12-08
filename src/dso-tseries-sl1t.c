/*** dso-tseries-sl1t.c -- sl1t tick files
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

#include <sushi/sl1t.h>
#include <sushi/sl1tfile.h>
/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "unserding-private.h"
/* for higher level packet handling */
#include "seria-proto-glue.h"
#include "tseries-private.h"

#if defined DEBUG_FLAG
# define UD_DEBUG_TSER(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */

typedef struct tssl1t_s *tssl1t_t;

struct tssl1t_s {
	int infd;
	void *rdr;
};


static const char fxsl1tf[] = "/home/freundt/.unserding/EUR_hist.sl1t";

static void
load_sl1t_file(tssl1t_t ctx)
{
	if ((ctx->infd = open(fxsl1tf, O_RDONLY, 0644)) < 0) {
		/* file not found */
		ctx->rdr = NULL;
		return;
	}
	ctx->rdr = make_sl1t_reader(ctx->infd);
	return;
}


static struct tssl1t_s my_ctx[1];

void
dso_tseries_sl1t_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-sl1t: loading ...");
	load_sl1t_file(my_ctx);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_sl1t_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-sl1t: unloading ...");
	if (my_ctx->rdr != NULL) {
		free_sl1t_reader(my_ctx->rdr);
	}
	close(my_ctx->infd);
	UD_DBGCONT("done\n");
	return;
}

/* dso-sl1t.c */
