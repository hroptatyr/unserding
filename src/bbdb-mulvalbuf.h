/*** bbdb-mulvalbuf.h -- buffer for multiple string values
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <hroptatyr@sxemacs.org>
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

#if !defined INCLUDED_bbdb_mulvalbuf_h_
#define INCLUDED_bbdb_mulvalbuf_h_

#include <stdlib.h>
#include <stdint.h>

#define MVBINC			32
#define RNDU64(_x)		((((_x) / MVBINC) * MVBINC) + MVBINC)
#define RNDD64(_x)		((((_x) / MVBINC) * MVBINC))

typedef struct mvbuf_s *mvbuf_t;
typedef uint32_t mvbsize_t;

struct mvbuf_s {
	char *buf;
	mvbsize_t nvals;
	mvbsize_t cursor;
	mvbsize_t alllen;
};

static inline void
init_mvbuf(mvbuf_t mvb)
{
	mvb->buf = malloc(RNDU64(0));
	memset(mvb->buf, 0, RNDU64(0));
	return;
}

static inline void
free_mvbuf(mvbuf_t mvb)
{
	free(mvb->buf);
	memset(mvb, 0, sizeof(*mvb));
	return;
}

static inline const char __attribute__((always_inline))*
mvbuf_buffer(mvbuf_t mvb)
{
	return mvb->buf;
}

static inline mvbsize_t __attribute__((always_inline))
mvbuf_nvals(mvbuf_t mvb)
{
	return mvb->nvals;
}

static inline mvbsize_t __attribute__((always_inline))
mvbuf_buffer_len(mvbuf_t mvb)
{
	return mvb->cursor;
}

static inline mvbsize_t __attribute__((always_inline))
mvbuf_alloc_len(mvbuf_t mvb)
{
	return mvb->alllen;
}

static inline void __attribute__((always_inline))
mvbuf_add(mvbuf_t mvb, const char *s, mvbsize_t len)
{
	mvbsize_t newlen = mvb->cursor + len + 1;

	if (RNDU64(newlen) > RNDU64(mvb->cursor)) {
		mvb->buf = realloc(mvb->buf, RNDU64(newlen));
	}
	memcpy(&mvb->buf[mvb->cursor], s, len);
	mvb->buf[newlen - 1] = '\000';
	mvb->alllen = mvb->cursor = newlen;
	mvb->nvals++;
	return;
}

static inline mvbsize_t __attribute__((always_inline))
mvbuf_vals(const char **buf, mvbuf_t mvb, mvbsize_t idx)
{
	for (mvbsize_t len = 0, i = 0; i < mvb->nvals; i++) {
		mvbsize_t tmp = strlen(&mvb->buf[len]);
		if (i == idx) {
			*buf = &mvb->buf[len];
			return tmp;
		}
		len += tmp + 1;
	}
	*buf = NULL;
	return 0;
}

#endif	/* INCLUDED_bbdb_mulvalbuf_h_ */
