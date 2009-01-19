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
#include "catalogue.h"
#include "protocore.h"

typedef struct ud_cat_s *__cat_t;

static const char empty_msg[] = "empty\n";

/* the global catalogue */
static ud_cat_t ud_catalogue = NULL;
static size_t ud_catalen = 0;


/* helpers */
void
__ud_fill_catobj(ud_catobj_t co, ...)
{
	va_list args;

        /* prepare list for va_arg */
        va_start(args, co);
	(void)va_arg(args, void*);
        for (uint8_t i = 0; i < co->nattrs; ++i ) {
		co->attrs[i] = va_arg(args, ud_tlv_t);
	}
	va_end(args);
	return;
}

void
ud_cat_add_obj(ud_catobj_t co)
{
	ud_cat_t new = malloc(sizeof(struct ud_cat_s));
	new->next = ud_catalogue;
	new->data = co;
	ud_catalen++;
	return;
}

static inline size_t __attribute__((always_inline))
snprintcat(char *restrict buf, size_t blen, const __cat_t c)
{
	size_t len = snprintf(buf, blen, "---- %p %s\n",
			      c->data, (const char*)c->data);
#if 0
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
#endif
	return len;
}

static size_t
serialise_keyval(char *restrict buf, ud_tlv_t keyval)
{
	size_t idx = 1;
	/* refactor me, lookup table? */
	switch ((ud_tag_t)(buf[0] = keyval->tag)) {
	case UD_TAG_CLASS:
		/* should be serialise_class */
		memcpy(&buf[idx], keyval, keyval->data[0]);
		idx += keyval->data[0] + 1;
		break;
	case UD_TAG_NAME:
		/* should be serialise_name */
		memcpy(&buf[idx], keyval, keyval->data[0]);
		idx += keyval->data[0] + 1;
		break;
	default:
		break;
	}
	return idx;
}

static size_t
serialise_catobj(char *restrict buf, ud_catobj_t co)
{
	size_t idx = 2;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_CATOBJ;
	buf[1] = co->nattrs;
	for (uint8_t i = 0; i < co->nattrs; ++i) {
		idx += serialise_keyval(&buf[2], co->attrs[i]);
	}
	return idx;
}

/* some jobs to browse the catalogue */
extern bool ud_cat_ls_job(job_t j);
bool
ud_cat_ls_job(job_t j)
{
	size_t idx = 10;

	UD_DEBUG_CAT("ls job\n");
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;
	/* we are ud_catalen entries wide */
	j->buf[9] = (uint8_t)ud_catalen;
#if 1
	for (ud_cat_t c = ud_catalogue; c; c = c->next) {
		ud_catobj_t dat = c->data;
		idx += serialise_catobj(&j->buf[idx], dat);
	}
#endif
	j->blen = idx;
	return false;
}

/* catalogue.c ends here */
