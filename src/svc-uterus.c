/*** svc-uterus.c -- uterus codec
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
/* to get a take on them m30s and m62s */
# define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
# include <uterus/m62.h>
# define HAVE_UTERUS
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
# include <m62.h>
# define HAVE_UTERUS
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#include "unserding.h"
#include "unserding-nifty.h"
#include "boobs.h"

#include "svc-uterus.h"

#if defined UNSERMON_DSO
# include "unsermon.h"
#endif	/* UNSERMON_DSO */


/* helpers */
static inline size_t
pr_tsmstz(char *restrict buf, time_t sec, uint32_t msec, char sep)
{
	struct tm *tm;

	tm = gmtime(&sec);
	strftime(buf, 32, "%F %T", tm);
	buf[10] = sep;
	buf[19] = '.';
	buf[20] = (char)(((msec / 100) % 10) + '0');
	buf[21] = (char)(((msec / 10) % 10) + '0');
	buf[22] = (char)(((msec / 1) % 10) + '0');
	buf[23] = '+';
	buf[24] = '0';
	buf[25] = '0';
	buf[26] = ':';
	buf[27] = '0';
	buf[28] = '0';
	return 29;
}

static inline size_t
pr_ttf(char *restrict buf, uint16_t ttf)
{
	switch (ttf & ~(SCOM_FLAG_LM | SCOM_FLAG_L2M)) {
	case SL1T_TTF_BID:
		*buf = 'b';
		break;
	case SL1T_TTF_ASK:
		*buf = 'a';
		break;
	case SL1T_TTF_TRA:
		*buf = 't';
		break;
	case SL1T_TTF_FIX:
		*buf = 'f';
		break;
	case SL1T_TTF_STL:
		*buf = 'x';
		break;
	case SL1T_TTF_AUC:
		*buf = 'k';
		break;

	case SBAP_FLAVOUR:
		*buf++ = 'b';
		*buf++ = 'a';
		*buf++ = 'p';
		return 3;

	case SCOM_TTF_UNK:
	default:
		*buf++ = '?';
		break;
	}
	return 1;
}

static size_t
__pr_snap(char *tgt, scom_t st)
{
	const_ssnp_t snp = (const void*)st;
	char *p = tgt;

	/* bid price */
	p += ffff_m30_s(p, (m30_t)snp->bp);
	*p++ = '\t';
	/* ask price */
	p += ffff_m30_s(p, (m30_t)snp->ap);
	if (scom_thdr_ttf(st) == SSNP_FLAVOUR) {
		/* real snaps reach out further */
		*p++ = '\t';
		/* bid quantity */
		p += ffff_m30_s(p, (m30_t)snp->bq);
		*p++ = '\t';
		/* ask quantity */
		p += ffff_m30_s(p, (m30_t)snp->aq);
		*p++ = '\t';
		/* volume-weighted trade price */
		p += ffff_m30_s(p, (m30_t)snp->tvpr);
		*p++ = '\t';
		/* trade quantity */
		p += ffff_m30_s(p, (m30_t)snp->tq);
	}
	return p - tgt;
}

static size_t
__attribute__((noinline))
__pr_cdl(char *tgt, scom_t st)
{
	const_scdl_t cdl = (const void*)st;
	char *p = tgt;

	/* h(igh) */
	p += ffff_m30_s(p, (m30_t)cdl->h);
	*p++ = '\t';
	/* l(ow) */
	p += ffff_m30_s(p, (m30_t)cdl->l);
	*p++ = '\t';
	/* o(pen) */
	p += ffff_m30_s(p, (m30_t)cdl->o);
	*p++ = '\t';
	/* c(lose) */
	p += ffff_m30_s(p, (m30_t)cdl->c);
	*p++ = '\t';
	/* start of the candle */
	p += sprintf(p, "%08x", cdl->sta_ts);
	*p++ = '|';
	p += pr_tsmstz(p, cdl->sta_ts, 0, 'T');
	*p++ = '\t';
	/* event count in candle, print 3 times */
	p += sprintf(p, "%08x", cdl->cnt);
	*p++ = '|';
	p += ffff_m30_s(p, (m30_t)cdl->cnt);
	return p - tgt;
}


/* packing service */
#if !defined UNSERMON_DSO
#endif	/* UNSERMON_DSO */


/* monitor service */
#if defined UNSERMON_DSO
static size_t
mon_dec_7572(
	char *restrict p, size_t z, ud_svc_t UNUSED(svc),
	const struct ud_msg_s m[static 1])
{
	struct __brag_wire_s {
		uint16_t idx;
		uint8_t syz;
		uint8_t urz;
		char symuri[256 - sizeof(uint32_t)];
	};
	const struct __brag_wire_s *msg = m->data;
	char *restrict q = p;

	if ((size_t)msg->syz + (size_t)msg->urz > sizeof(msg->symuri)) {
		/* shouldn't be */
		return 0;
	}

	q += snprintf(q, z - (q - p), "%hu\t", htobe16(msg->idx));
	memcpy(q, msg->symuri, msg->syz);
	q += msg->syz;

	*q++ = '\t';
	memcpy(q, msg->symuri + msg->syz, msg->urz);
	q += msg->urz;
	return q - p;
}

static size_t
mon_dec_7574(
	char *restrict p, size_t UNUSED(z), ud_svc_t UNUSED(svc),
	const struct ud_msg_s m[static 1])
{
	char *restrict q = p;
#if defined HAVE_UTERUS
	scom_t sp = m->data;
	uint32_t sec = scom_thdr_sec(sp);
	uint16_t msec = scom_thdr_msec(sp);
	uint16_t tidx = scom_thdr_tblidx(sp);
	uint16_t ttf = scom_thdr_ttf(sp);

	/* big time stamp upfront */
	q += pr_tsmstz(q, sec, msec, 'T');
	*q++ = '\t';
	/* index into symtable */
	q += snprintf(q, z - (q - p), "%x", tidx);
	*q++ = '\t';
	/* tick type */
	q += pr_ttf(q, ttf);
	*q++ = '\t';
	switch (ttf) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
	case SL1T_TTF_FIX:
	case SL1T_TTF_STL:
	case SL1T_TTF_AUC:
#if defined SL1T_TTF_G32
	case SL1T_TTF_G32:
#endif	/* SL1T_TTF_G32 */
	case SL2T_TTF_BID:
	case SL2T_TTF_ASK:
		/* just 2 generic m30s */
		if (LIKELY(m->dlen == sizeof(struct sl1t_s))) {
			const_sl1t_t t = m->data;
			q += ffff_m30_s(q, (m30_t)t->v[0]);
			*q++ = '\t';
			q += ffff_m30_s(q, (m30_t)t->v[1]);
		}
		break;
	case SL1T_TTF_VOL:
	case SL1T_TTF_VPR:
	case SL1T_TTF_VWP:
	case SL1T_TTF_OI:
#if defined SL1T_TTF_G64
	case SL1T_TTF_G64:
#endif	/* SL1T_TTF_G64 */
		/* one giant m62 */
		if (LIKELY(m->dlen == sizeof(struct sl1t_s))) {
			const_sl1t_t t = m->data;
			q += ffff_m62_s(q, (m62_t)t->w[0]);
		}
		break;

		/* snaps */
	case SSNP_FLAVOUR:
	case SBAP_FLAVOUR:
		if (LIKELY(m->dlen == sizeof(struct ssnp_s))) {
			q += __pr_snap(q, sp);
		}
		break;

		/* candles */
	case SL1T_TTF_BID | SCOM_FLAG_LM:
	case SL1T_TTF_ASK | SCOM_FLAG_LM:
	case SL1T_TTF_TRA | SCOM_FLAG_LM:
	case SL1T_TTF_FIX | SCOM_FLAG_LM:
	case SL1T_TTF_STL | SCOM_FLAG_LM:
	case SL1T_TTF_AUC | SCOM_FLAG_LM:
		if (LIKELY(m->dlen == sizeof(struct scdl_s))) {
			q += __pr_cdl(q, sp);
		}
		break;

	case SCOM_TTF_UNK:
	default:
		break;
	}
#endif	/* HAVE_UTERUS */

	return q - p;
}

int
svc_uterus_LTX_ud_mondec_init(void)
{
	ud_mondec_reg(0x7572, mon_dec_7572);
	ud_mondec_reg(0x7573, mon_dec_7572);
	ud_mondec_reg(0x7574, mon_dec_7574);
	ud_mondec_reg(0x7575, mon_dec_7574);
	return 0;
}

int ud_mondec_init(void)
	__attribute__((alias("svc_uterus_LTX_ud_mondec_init")));
#endif	/* UNSERMON_DSO */

/* svc-uterus.c ends here */
