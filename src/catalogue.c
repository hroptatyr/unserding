/*** catalogue.c -- unserding catalogue
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
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"

typedef struct ud_cat_s *__cat_t;

static const char empty_msg[] = "empty\n";

#define countof_m1(_x)		(countof(_x) - 1)

/* the global catalogue */
static struct ud_cat_s __ud_catalogue = {
	.parent = NULL,
	.fir_child = NULL,
	.las_child = NULL,
	.next = NULL,
	.prev = NULL,
	.data = "catalogue",
	.flags = UD_CF_JUSTCAT,
};
ud_cat_t ud_catalogue = &__ud_catalogue;


/* helpers */
static inline size_t __attribute__((always_inline))
snprintcat(char *restrict buf, size_t blen, const __cat_t c)
{
	size_t len = snprintf(buf, blen, "---- %p %s\n",
			      c->data, (const char*)c->data);
	if (ud_cat_justcatp(c)) {
		buf[3] = 'c';
	}
	if (ud_cat_spottablep(c)) {
		buf[2] = 's';
	}
	if (ud_cat_tradablep(c)) {
		buf[1] = 't';
	}
	if (ud_cat_lastp(c)) {
		buf[0] = 'l';
	}
	return len;
}

/* some jobs to browse the catalogue */
void
ud_cat_ls_job(job_t j)
{
	size_t idx = 0;

	UD_DEBUG_CAT("ls job\n");
#if 0
	for (__cat_t c = ud_cat_first_child(ctx->pwd); c; c = c->next) {
		idx += snprintcat(j->buf + idx, JOB_BUF_SIZE - idx, c);
	}
#endif
	if (UNLIKELY((j->blen = idx) == 0)) {
		memcpy(j->buf, empty_msg, j->blen = countof_m1(empty_msg));
	}
	j->prntf(EV_DEFAULT_ j);
	return;
}

void
ud_cat_pwd_job(job_t j)
{
	size_t UNUSED(len);

	UD_DEBUG_CAT("pwd job\n");
#if 0
	len = snprintcat(j->buf, JOB_BUF_SIZE, ctx->pwd);
#else
	j->blen = snprintf(j->buf, JOB_BUF_SIZE, "No working catalogue\n");
#endif
	j->prntf(EV_DEFAULT_ j);
	return;
}


void
ud_cat_cd_job(job_t j)
{
	const char *UNUSED(dir) = j->buf;
	size_t UNUSED(len) = 0;
	static const char err[] = "No such catalogue entry\n";

	UD_DEBUG_CAT("cd job\n");
#if 0
	/* check for the special cat symbols `.' and `..' */
	if (UNLIKELY(dir[0] == '.')) {
		if (LIKELY(dir[1] == '.')) {
			ctx->pwd = ud_cat_parent(ctx->pwd);
			len = snprintcat(j->work_space, 256, ctx->pwd);
		}
		len = snprintcat(j->work_space, 256, ctx->pwd);
		goto out;
	}

	for (__cat_t c = ud_cat_first_child(ctx->pwd); c; c = c->next) {
		const char *tmp = ud_cat_data(c);
		if (UNLIKELY(strcmp(dir, tmp) == 0)) {
			ctx->pwd = c;
			len = snprintcat(j->work_space, 256, c);
			goto out;
		}
	}
#endif
	memcpy(j->buf, err, j->blen = countof_m1(err));
	
out:
	j->prntf(EV_DEFAULT_ j);
	return;
}

/* catalogue.c ends here */
