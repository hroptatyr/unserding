/*** seria.c -- unserding serialisation
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#include <stdint.h>
#include "seria.h"

#define SERIA_MAX_DEPTH		8


uint16_t
udpc_msg_size(const char *sig)
{
	uint16_t res[SERIA_MAX_DEPTH];
	uint8_t cnt[SERIA_MAX_DEPTH];
	uint8_t idx = 0;

	res[0] = 0;
	cnt[0] = 1;
	for (const uint8_t *c = (const void*)sig; *c; c++) {
		if (!(UDPC_SEQ_MASK & *c) &&
		    ((*c & UDPC_SIZ_MASK) != UDPC_TYPE_REC)) {
			/* simple type */
			res[idx] += (uint16_t)(*c & UDPC_SIZ_MASK);
		} else if ((UDPC_SEQ_MASK & *c) &&
			   ((*c & UDPC_SIZ_MASK) != UDPC_TYPE_REC)) {
			/* seqof simple type */
			res[idx] += (uint16_t)(*c & UDPC_SIZ_MASK) * c[1];
			c++;
		} else if ((UDPC_SEQ_MASK & *c)) {
			/* seqof(struct ...) */
			idx++;
			res[idx] = 0;
			cnt[idx] = c[1];
			c++;
		} else if (!(*c & UDPC_SGN_MASK)) {
			/* must be a struct */
			idx++;
			res[idx] = 0;
			cnt[idx] = 1;
		} else {
			/* must be the end of a structs */
			uint16_t sz = cnt[idx] * res[idx];
			if (idx-- == 0) {
				return 0;
			}
			res[idx] += sz;
		}
	}
	return res[0];
}

void
udpc_sig_string(char *restrict out, const char *sig)
{
	for (const uint8_t *c = (const void*)sig; *c; c++) {
		if (!(UDPC_SEQ_MASK & *c) &&
		    ((*c & UDPC_SIZ_MASK) != UDPC_TYPE_REC)) {
			/* simple type */
			*out++ = (*c & UDPC_SIZ_MASK) + 'a';
		} else if ((UDPC_SEQ_MASK & *c) &&
			   ((*c & UDPC_SIZ_MASK) != UDPC_TYPE_REC)) {
			/* seqof simple type */
			*out++ = (c[1] & 0x0f) + '0';
			*out++ = (*c & UDPC_SIZ_MASK) + 'a';
			c++;
		} else if ((UDPC_SEQ_MASK & *c)) {
			/* seqof(struct ...) */
			*out++ = (c[1] & 0x0f) + '0';
			*out++ = '(';
			c++;
		} else if (!(*c & UDPC_SGN_MASK)) {
			/* must be a struct */
			*out++ = '(';
		} else {
			/* must be the end of a structs */
			*out++ = ')';
		}
	}
	*out = '\0';
	return;
}

void
udpc_fprint_msg(FILE *out, const char *msg)
{
	return;
}

/* seria.c ends here */
