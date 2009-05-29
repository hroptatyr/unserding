/*** seria.h -- unserding serialisation
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

#if !defined INCLUDED_seria_h_
#define INCLUDED_seria_h_

#include <stdio.h>
#include <string.h>

/* old shit, to be deleted */
#define UDPC_TYPE_SEQ	'a'
#define UDPC_TYPE_VAR	'v'


/* one byte sequences */
#define UDPC_TYPE_UNK	0x00
#define UDPC_TYPE_BYTE	0x01
#define UDPC_TYPE_UI16	0x02
#define UDPC_TYPE_SI16	(UDPC_TYPE_UI16 | UDPC_SGN_MASK)
#define UDPC_TYPE_UI32	0x04
#define UDPC_TYPE_SI32	(UDPC_TYPE_UI32 | UDPC_SGN_MASK)
#define UDPC_TYPE_UI64	0x08
#define UDPC_TYPE_SI64	(UDPC_TYPE_UI64 | UDPC_SGN_MASK)

#define UDPC_TYPE_FLTH	(UDPC_TYPE_UI16 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTS	(UDPC_TYPE_UI32 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTD	(UDPC_TYPE_UI64 | UDPC_FLT_MASK)

#define UDPC_TYPE_STR	(UDPC_TYPE_BYTE | UDPC_SEQ_MASK)
#define UDPC_TYPE_NSTR(_n)	(UDPC_TYPE_BYTE | UDPC_SEQ_MASK), _n

/* multi byte sigs */
#define UDPC_TYPE_REC	0x0f	/* + slot sigs */
#define UDPC_TYPE_EOR	(UDPC_TYPE_REC | UDPC_SGN_MASK)	/* end of struct */

/* masks */
#define UDPC_SGN_MASK	0x10
#define UDPC_FLT_MASK	0x20
#define UDPC_SEQ_MASK	0x80	/* must be \nul terminated */
#define UDPC_SIZ_MASK	0x0f

#define UDPC_SEQOF(_x)	(UDPC_TYPE_##_x | UDPC_SEQ_MASK)

typedef struct udpc_seria_s *udpc_seria_t;
struct udpc_seria_s {
	char *msg;
	uint16_t len;
	uint16_t msgoff;
};

/* tag-length-offset struct */
struct udpc_tlo_s {
	uint8_t tag;
	uint8_t length;
	uint16_t offset;
};

static inline void
udpc_seria_init(udpc_seria_t sctx, char *buf, uint16_t len)
{
	sctx->msg = buf;
	sctx->len = len;
	sctx->msgoff = 0;
	return;
}

static inline uint16_t
udpc_seria_msglen(udpc_seria_t sctx)
{
	return sctx->msgoff;
}


/* serialisers/deserialiser */
/* return the next _ALGN aligned address after _CUR */
#if !defined ROUND
# define ROUND(s, a)		(a * ((s + a - 1) / a))
#endif	/* !ROUND */

#define UDPC_NEW_SERIA(_na, _NA, _ty)					\
static inline void							\
udpc_seria_add_##_na(udpc_seria_t sctx, _ty v)				\
{									\
	uint16_t off = ROUND(sctx->msgoff + 1, __alignof__(_ty));	\
	_ty *p = (_ty*)&sctx->msg[off];					\
									\
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_##_NA;			\
	*p = v;								\
	sctx->msgoff = off + sizeof(_ty);				\
	return;								\
}									\
									\
static inline _ty							\
udpc_seria_des_##_na(udpc_seria_t sctx)					\
{									\
	uint16_t off = ROUND(sctx->msgoff + 1, __alignof__(_ty));	\
	_ty *p = (_ty*)&sctx->msg[off];					\
									\
	sctx->msgoff = off + sizeof(_ty);				\
	return *p;							\
}									\
									\
static inline void							\
udpc_seria_add_seq##_na(udpc_seria_t sctx, const _ty *s, size_t len)	\
{									\
	uint16_t off = ROUND(sctx->msgoff + 2, __alignof__(s));		\
	_ty *p = (_ty*)&sctx->msg[off];					\
									\
	sctx->msg[sctx->msgoff + 0] = UDPC_SEQOF(_NA);			\
	sctx->msg[sctx->msgoff + 1] = (uint8_t)len;			\
	memcpy(p, s, len * sizeof(*s));					\
	sctx->msgoff = off + len * sizeof(*s);				\
	return;								\
}									\
									\
static inline size_t							\
udpc_seria_des_seq##_na(udpc_seria_t sctx, const _ty **s)		\
{									\
	uint16_t off = ROUND(sctx->msgoff + 2, __alignof__(*s));	\
	size_t len = (size_t)(uint8_t)sctx->msg[sctx->msgoff+1];	\
									\
	*s = (_ty*)&sctx->msg[off];					\
	sctx->msgoff = off + sizeof(_ty) * len;				\
	return len;							\
}									\
									\
static inline size_t							\
udpc_seria_des_seq##_na##_copy(_ty **out, udpc_seria_t sctx)		\
{									\
	uint16_t off = ROUND(sctx->msgoff + 2, __alignof__(*out));	\
	size_t len = (size_t)(uint8_t)sctx->msg[sctx->msgoff+1];	\
									\
	*out = malloc(sizeof(_ty) * len);				\
	memcpy(*out, (_ty*)&sctx->msg[off], sizeof(_ty) * len);		\
	sctx->msgoff = off + sizeof(_ty) * len;				\
	return len;							\
}


UDPC_NEW_SERIA(byte, BYTE, uint8_t);
UDPC_NEW_SERIA(ui16, UI16, uint16_t);
UDPC_NEW_SERIA(si16, SI16, int16_t);
UDPC_NEW_SERIA(ui32, UI32, uint32_t);
UDPC_NEW_SERIA(si32, SI32, int32_t);
UDPC_NEW_SERIA(ui64, UI64, uint64_t);
UDPC_NEW_SERIA(si64, SI64, int64_t);
/* floats */
UDPC_NEW_SERIA(flts, FLTS, float);
UDPC_NEW_SERIA(fltd, FLTD, double);

/* seqof(byte) */
static inline void
udpc_seria_add_str(udpc_seria_t sctx, const char *s, size_t len)
{
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_STR;
	sctx->msg[sctx->msgoff + 1] = (uint8_t)len;
	memcpy(&sctx->msg[sctx->msgoff + 2], s, len);
	sctx->msgoff += 2 + len;
	return;
}

static inline size_t
udpc_seria_des_str(udpc_seria_t sctx, const char **s)
{
	size_t len = (size_t)(uint8_t)sctx->msg[sctx->msgoff+1];
	*s = &sctx->msg[sctx->msgoff+2];
	sctx->msgoff += 2 + len;
	return len;
}

static inline uint8_t
udpc_seria_tag(udpc_seria_t sctx)
{
	if (sctx->msgoff < sctx->len) {
		return sctx->msg[sctx->msgoff];
	}
	return UDPC_TYPE_UNK;
}


/* public funs */
#if defined SIG_UPFRONT
extern uint16_t udpc_msg_size(const char *sig);
extern void udpc_sig_string(char *restrict out, const char *sig);
#endif

#endif	/* INCLUDED_seria_h_ */
