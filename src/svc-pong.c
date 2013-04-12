/*** svc-pong.c -- pong service goodies
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
#include "unserding.h"
#include "svc-pong.h"
#include "ud-nifty.h"
#include "ud-private.h"
#include "boobs.h"

#if defined UNSERMON_DSO
# include "unsermon.h"
#endif	/* UNSERMON_DSO */

struct __ping_s {
	uint32_t pid;
	uint8_t hnz;
	char hn[];
};


/* packing service */
#if !defined UNSERMON_DSO
static union {
	struct __ping_s wire;
	char buf[64];
} __msg;

#define MAX_HNZ		((uint8_t)(sizeof(__msg) - 6U))

int
ud_pack_ping(ud_sock_t sock, const struct svc_ping_s msg[static 1])
{
	ud_svc_t cmd;

	switch (msg->what) {
	case SVC_PING_PING:
		cmd = UD_CTRL_SVC(UD_SVC_PING);
		break;
	case SVC_PING_PONG:
		cmd = UD_CTRL_SVC(UD_SVC_PING + 1/*reply*/);
		break;
	default:
		return -1;
	}

	/* 4 bytes for the pid */
	__msg.wire.pid = htobe32((uint32_t)msg->pid);
	if ((__msg.wire.hnz = (uint8_t)msg->hostnlen) > MAX_HNZ) {
		__msg.wire.hnz = sizeof(__msg) - 5;
	}
	memcpy(__msg.wire.hn, msg->hostname, __msg.wire.hnz);

	(void)ud_flush(sock);
	return ud_pack_cmsg(sock, (struct ud_msg_s){
			.svc = cmd,
			.data = __msg.buf,
			.dlen = __msg.wire.hn - __msg.buf + __msg.wire.hnz,
		});
}

int
ud_pack_pong(ud_sock_t sock, unsigned int pongp)
{
/* PINGs can't be packed. */
	static struct svc_ping_s po;

	if (UNLIKELY(po.hostnlen == 0U)) {
		static char hname[_POSIX_HOST_NAME_MAX];
		if (gethostname(hname, sizeof(hname)) < 0) {
			return -1;
		}
		hname[_POSIX_HOST_NAME_MAX - 1] = '\0';
		po.hostnlen = strlen(hname);
		po.hostname = hname;
		po.pid = getpid();
	}

	if (pongp) {
		po.what = SVC_PING_PONG;
	} else {
		po.what = SVC_PING_PING;
	}
	return ud_pack_ping(sock, &po);
}

int
ud_chck_ping(struct svc_ping_s *restrict tgt, ud_sock_t sock)
{
	struct ud_msg_s msg[1];

	if (ud_chck_msg(msg, sock) < 0) {
		return -1;
	} else if (msg->dlen > sizeof(__msg)) {
		return -1;
	} else if ((msg->svc & ~0x01) != UD_CTRL_SVC(UD_SVC_PING)) {
		/* not a PING nor a PONG */
		return -1;
	}
	/* otherwise memcpy to static buffer for inspection */
	memcpy(__msg.buf, msg->data, msg->dlen);
	if (__msg.wire.hnz > MAX_HNZ) {
		return -1;
	}
	tgt->hostnlen = __msg.wire.hnz;
	tgt->hostname = __msg.wire.hn;
	tgt->pid = be32toh(__msg.wire.pid);
	tgt->what = msg->svc & 0x01 ? SVC_PING_PONG : SVC_PING_PING;
	/* as a service, \nul terminate the hostname */
	__msg.wire.hn[__msg.wire.hnz] = '\0';
	return 0;
}
#endif	/* !UNSERMON_DSO */


/* monitor service */
#if defined UNSERMON_DSO
static size_t
mon_dec_ping(
	char *restrict p, size_t z, ud_svc_t svc,
	const struct ud_msg_s m[static 1])
{
	static const char ping[] = "PING";
	static const char pong[] = "PONG";
	char *restrict q = p;

	switch (svc) {
	case UD_CTRL_SVC(UD_SVC_PING):
		memcpy(q, ping, sizeof(ping));
		break;
	case UD_CTRL_SVC(UD_SVC_PING + 1):
		memcpy(q, pong, sizeof(pong));
		break;
	default:
		return 0UL;
	}
	(q += sizeof(ping))[-1] = '\t';

	/* decipher the actual message */
	const union {
		struct __ping_s wire;
		char buf[64];
	} *pm;

	if (UNLIKELY((pm = m->data) == NULL || m->dlen != sizeof(*pm))) {
		*q++ = '?';
	} else {
		q += snprintf(q, z - (q - p), "%u\t", be32toh(pm->wire.pid));
		memcpy(q, pm->wire.hn, pm->wire.hnz);
		q += pm->wire.hnz;
	}
	return q - p;
}

int
ud_mondec_init(void)
{
	ud_mondec_reg(UD_CTRL_SVC(UD_SVC_PING), mon_dec_ping);
	ud_mondec_reg(UD_CTRL_SVC(UD_SVC_PING + 1), mon_dec_ping);
	return 0;
}
#endif	/* UNSERMON_DSO */

/* svc-pong.c ends here */
