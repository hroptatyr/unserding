/*** ud-private.h -- private API definitions
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
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
#if !defined INCLUDED_ud_private_h_
#define INCLUDED_ud_private_h_

#include <stdint.h>
#include "unserding.h"
#include "ud-sockaddr.h"

/**
 * Message for auxiliary data. */
struct ud_auxmsg_s {
	const struct ud_sockaddr_s *src;
	uint16_t pno;
	ud_svc_t svc;
	uint32_t len;
};


/**
 * Pack a control message for instant transmission in S. */
extern int ud_pack_cmsg(ud_sock_t s, struct ud_msg_s msg);

/**
 * Scan messages in S for control messages and take actions. */
extern int ud_chck_cmsg(struct ud_msg_s *restrict tgt, ud_sock_t s);

/**
 * For current messages in S fill in the auxmsg object TGT. */
extern int ud_chck_aux(struct ud_auxmsg_s *restrict tgt, ud_sock_t s);


/* specific services */
/**
 * Control channel with no user defined messages. */
#define UD_CHN_CTRL	0xffU

#define UD_CHN(s)	((s) / 0x100U)
#define UD_SVC(c, s)	((c) * 0x100U + (s))

#define UD_CTRL_SVC(s)	UD_SVC(UD_CHN_CTRL, s)

enum chn_ctrl_e {
	/** command request/announce service to discover new commands */
	UD_SVC_CMD = 0x00U,
	/** time request/reply service to get an idea about network lags */
	UD_SVC_TIME = 0x02U,
	/** Ping/pong service to determine neighbours */
	UD_SVC_PING = 0x04U,
};

#endif	/* INCLUDED_ud_private_h_ */
