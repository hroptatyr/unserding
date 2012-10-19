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

#if defined __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

/* old shit, to be deleted */
#define UDPC_TYPE_SEQ	'a'
#define UDPC_TYPE_VAR	'v'

#if defined __INTEL_COMPILER
/* icc remark #2259 is:
 * non-pointer conversion from "unsigned int" to
 * "uint16_t={unsigned short}" may lose significant bits */
# pragma warning	(disable:2259)
#endif	/* __INTEL_COMPILER */

#if !defined MIN
# define MIN(a, b)	a < b ? a : b;
#endif	/* !MIN */


/* one byte sequences */
#define UDPC_TYPE_UNK	0x00
#define UDPC_TYPE_BYTE	0x01
#define UDPC_TYPE_UI16	0x02
#define UDPC_TYPE_UI32	0x04
#define UDPC_TYPE_UI64	0x08

#define UDPC_TYPE_FLTH	(UDPC_TYPE_UI16 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTS	(UDPC_TYPE_UI32 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTD	(UDPC_TYPE_UI64 | UDPC_FLT_MASK)

#define UDPC_TYPE_STR	(UDPC_TYPE_BYTE | UDPC_SEQ_MASK)
#define UDPC_TYPE_NSTR(_n)	(UDPC_TYPE_BYTE | UDPC_SEQ_MASK), _n

/* for generic data objects */
#define UDPC_TYPE_DATA	(0x0c)
/* for XDR objects */
#define UDPC_TYPE_XDR	(0x0d)
/* for ASN.1 objects */
#define UDPC_TYPE_ASN1	(0x0e)

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
	if (udpc_seria_tag(sctx) != UDPC_TYPE_##_NA) {			\
		return 0;						\
	}								\
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
	size_t len;							\
									\
	if (udpc_seria_tag(sctx) != UDPC_SEQOF(_NA)) {			\
		*s = NULL;						\
		return 0;						\
	}								\
	len = (size_t)(uint8_t)sctx->msg[sctx->msgoff+1];		\
	*s = (_ty*)&sctx->msg[off];					\
	sctx->msgoff = off + sizeof(_ty) * len;				\
	return len;							\
}

static inline uint8_t
udpc_seria_tag(udpc_seria_t sctx)
{
	if (sctx->msgoff < sctx->len) {
		return sctx->msg[sctx->msgoff];
	}
	return UDPC_TYPE_UNK;
}

#if defined __INTEL_COMPILER
/* we know there's a orphan ; wandering around when using NEW_SERIA */
#pragma warning (disable:424)
#endif	/* __INTEL_COMPILER */

UDPC_NEW_SERIA(byte, BYTE, uint8_t);
UDPC_NEW_SERIA(ui16, UI16, uint16_t);
UDPC_NEW_SERIA(ui32, UI32, uint32_t);
UDPC_NEW_SERIA(ui64, UI64, uint64_t);
/* floats */
UDPC_NEW_SERIA(flts, FLTS, float);
UDPC_NEW_SERIA(fltd, FLTD, double);

#if defined __INTEL_COMPILER
/* we know there's a orphan ; wandering around when using NEW_SERIA */
#pragma warning (default:424)
#endif	/* __INTEL_COMPILER */

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

/* fragment the string */
static inline size_t
udpc_seria_add_fragstr(udpc_seria_t sctx, const char *s, size_t len)
{
/* fs being the frag size which is now clamped to 252 */
	size_t fs = 252, i, pted = 0;

	for (i = 0, pted = 0; i < 5 && pted + fs < len; i++, pted += fs) {
		udpc_seria_add_str(sctx, &s[i*fs], fs);
	}
	if (i == 5) {
		return pted;
	} else {
		udpc_seria_add_str(sctx, &s[i*fs], len - pted);
		return len;
	}
}

static inline size_t
udpc_seria_des_str(udpc_seria_t sctx, const char **s)
{
	size_t len;
	if (udpc_seria_tag(sctx) != UDPC_TYPE_STR) {
		*s = NULL;
		return 0;
	}
	len = (size_t)(uint8_t)sctx->msg[sctx->msgoff+1];
	*s = &sctx->msg[sctx->msgoff+2];
	sctx->msgoff += 2 + len;
	return len;
}

static inline size_t
udpc_seria_des_str_into(char *s, size_t slen, udpc_seria_t sctx)
{
	const char *tgt;
	size_t cplen, deslen;
	deslen = udpc_seria_des_str(sctx, &tgt);
	cplen = MIN(deslen, slen - 1);
	memcpy(s, tgt, cplen);
	s[cplen] = '\0';
	return cplen;
}

/* XDR handling, could rename to opaque since essentially ASN.1 is
 * handled in the very same way */
#define XDR_HDR_LEN	(1 + 2)
static inline void
udpc_seria_add_xdr(udpc_seria_t sctx, const void *s, size_t len)
{
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_XDR;
	sctx->msg[sctx->msgoff + 1] = (uint8_t)(len >> 8);
	sctx->msg[sctx->msgoff + 2] = (uint8_t)(len & 0xff);
	memcpy(&sctx->msg[sctx->msgoff + XDR_HDR_LEN], s, len);
	sctx->msgoff += XDR_HDR_LEN + len;
	return;
}

static inline size_t
udpc_seria_des_xdr(udpc_seria_t sctx, const void **s)
{
	size_t len;
	if (udpc_seria_tag(sctx) != UDPC_TYPE_XDR) {
		*s = NULL;
		return 0;
	}
	len = (size_t)(((uint16_t)((uint8_t)sctx->msg[sctx->msgoff+1]) << 8) | \
		       ((uint8_t)sctx->msg[sctx->msgoff + 2]));
	*s = &sctx->msg[sctx->msgoff + XDR_HDR_LEN];
	sctx->msgoff += XDR_HDR_LEN + len;
	return len;
}

/* generic data handling, this is for very small objects,
 * preferably sans pointers and shite */
#define DATA_HDR_LEN	(1 + 1)
static inline void
udpc_seria_add_data(udpc_seria_t sctx, const void *s, uint8_t len)
{
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_DATA;
	sctx->msg[sctx->msgoff + 1] = len;
	memcpy(&sctx->msg[sctx->msgoff + DATA_HDR_LEN], s, len);
	sctx->msgoff += DATA_HDR_LEN + len;
	return;
}

static inline uint8_t
udpc_seria_des_data(udpc_seria_t sctx, const void **s)
{
	uint8_t len;
	if (udpc_seria_tag(sctx) != UDPC_TYPE_DATA) {
		*s = NULL;
		return 0;
	}
	len = (uint8_t)sctx->msg[sctx->msgoff+1];
	*s = &sctx->msg[sctx->msgoff + DATA_HDR_LEN];
	sctx->msgoff += DATA_HDR_LEN + len;
	return len;
}

static inline uint8_t
udpc_seria_des_data_into(void *s, size_t slen, udpc_seria_t sctx)
{
	const void *tgt;
	uint8_t cplen, deslen;
	deslen = udpc_seria_des_data(sctx, &tgt);
	cplen = MIN(deslen, slen);
	memcpy(s, tgt, cplen);
	return cplen;
}

/* ASN.1 handling */
static inline void
udpc_seria_add_asn1(udpc_seria_t sctx, const void *s, size_t len)
{
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_ASN1;
	sctx->msg[sctx->msgoff + 1] = (uint8_t)(len >> 8);
	sctx->msg[sctx->msgoff + 2] = (uint8_t)(len & 0xff);
	memcpy(&sctx->msg[sctx->msgoff + 3], s, len);
	sctx->msgoff += 3 + len;
	return;
}

static inline size_t
udpc_seria_des_asn1(udpc_seria_t sctx, const void **s)
{
	size_t len;
	if (udpc_seria_tag(sctx) != UDPC_TYPE_ASN1) {
		*s = NULL;
		return 0;
	}
	len = (size_t)(((uint16_t)((uint8_t)sctx->msg[sctx->msgoff+1]) << 8) | \
		       ((uint8_t)sctx->msg[sctx->msgoff + 2]));
	*s = &sctx->msg[sctx->msgoff + 3];
	sctx->msgoff += 3 + len;
	return len;
}

#if defined INCLUDED_unserding_h_
static inline ssize_t
ud_chan_send_ser(ud_chan_t chan, udpc_seria_t ser)
{
	size_t plen;

	if ((plen = udpc_seria_msglen(ser))) {
		ud_packet_t pkt = {UDPC_HDRLEN + plen, ser->msg - UDPC_HDRLEN};
		return ud_chan_send(chan, pkt);
	}
	return 0;
}
#endif	/* INCLUDED_unserding_h_ */


/* public funs */
#if defined SIG_UPFRONT
extern uint16_t udpc_msg_size(const char *sig);
extern void udpc_sig_string(char *restrict out, const char *sig);
#endif

#if defined __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_seria_h_ */
