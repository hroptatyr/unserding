/*** proto.c -- unserding protocol
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"

/***
 * The unserding protocol in detail:
 *
 * command: oi
 * replies: oi
 * purpose: announce yourself, can be used to fool the idle timer
 *
 * command: sup
 * replies: alright
 * purpose: return a list of cached queries
 *
 * command: cheers
 * replies: no worries
 * purpose: shows the unserding server how much you appreciated the
 *          last result set and makes him cache it for you
 *
 * command: wtf
 * replies: nvm
 * purpose: makes the unserding server forget about the last result
 *          set immediately
 *
 ***/

static const char inv_cmd[] = "invalid command, better luck next time\n";

static const char oi_cmd[] = "oi";
static const char oi_rpl[] = "oi\n";

static const char sup_cmd[] = "sup";
static const char sup_rpl[] = "alright\n";

static const char cheers_cmd[] = "cheers";
static const char cheers_rpl[] = "no worries\n";

static const char wtf_cmd[] = "wtf";
static const char wtf_rpl[] = "nvm\n";

void
ud_parse(job_t j)
{
/* clo is expected to be of type conn_ctx_t */
	conn_ctx_t ctx = j->clo;

	UD_DEBUG_PROTO("parsing: \"%s\"\n", j->work_space);

#define INNIT(_cmd)				\
	else if (memcmp(j->work_space, _cmd, countof(_cmd)) == 0)
	/* starting somewhat slowly with a memcmp */
	if (0) {
		;
	} INNIT(sup_cmd) {
		UD_DEBUG_PROTO("found `sup'\n");
		ud_print_tcp6(EV_DEFAULT_ ctx, sup_rpl, countof(sup_rpl)-1);

	} INNIT(oi_cmd) {
		UD_DEBUG_PROTO("found `oi'\n");
		ud_print_tcp6(EV_DEFAULT_ ctx, oi_rpl, countof(oi_rpl)-1);

	} INNIT(cheers_cmd) {
		UD_DEBUG_PROTO("found `cheers'\n");
		ud_print_tcp6(EV_DEFAULT_ ctx,
			      cheers_rpl, countof(cheers_rpl)-1);

	} INNIT(wtf_cmd) {
		UD_DEBUG_PROTO("found `wtf'\n");
		ud_print_tcp6(EV_DEFAULT_ ctx, wtf_rpl, countof(wtf_rpl)-1);

	} else {
		/* print an error */
		ud_print_tcp6(EV_DEFAULT_ ctx, inv_cmd, countof(inv_cmd)-1);
	}
#undef INNIT
	return;
}


/* proto.c ends here */
