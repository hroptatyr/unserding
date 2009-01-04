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

static const char empty_msg[] = "empty";

#define countof_m1(_x)		(countof(_x) - 1)

/* the global catalogue */
static struct ud_cat_s __ud_catalogue = {NULL, NULL, NULL, NULL, NULL, NULL};
ud_cat_t ud_catalogue = &__ud_catalogue;

/* some jobs to browse the catalogue */
void
ud_cat_ls_job(job_t j)
{
	conn_ctx_t ctx = j->clo;
	char *buf = malloc(4096);
	size_t idx = 0;

	for (__cat_t c = ud_cat_first_child(ud_catalogue); c; c = c->next) {
		size_t len = snprintf(buf + idx, 4095 - idx, "---- %p %s\n",
				      c->data, (const char*)c->data);
		if (ud_cat_justcatp(c)) {
			buf[idx + 3] = 'c';
		}
		idx += len;
	}
	if (UNLIKELY(idx == 0)) {
		memcpy(buf, empty_msg, idx = countof(empty_msg));
		buf[idx-1] = '\n';
	}
	ud_print_tcp6(EV_DEFAULT_ ctx, (void*)((long int)buf | 1UL), idx);
	return;
}

/* catalogue.c ends here */