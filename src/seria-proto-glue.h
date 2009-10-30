/*** seria-proto-glue.h -- useful stuff
 *
 * Copyright (C) 2009 Sebastian Freundt
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

#if !defined INCLUDED_seria_proto_glue_h_
#define INCLUDED_seria_proto_glue_h_

#include "protocore.h"
#include "seria.h"


/* higher level packet voodoo */
static inline void
clear_pkt(udpc_seria_t sctx, job_t rplj)
{
	memset(UDPC_PAYLOAD(rplj->buf), 0, UDPC_PLLEN);
	udpc_make_rpl_pkt(JOB_PACKET(rplj));
	udpc_seria_init(sctx, UDPC_PAYLOAD(rplj->buf), UDPC_PLLEN);
	return;
}

static inline void
copy_pkt(job_t tgtj, job_t srcj)
{
	memcpy(tgtj, srcj, sizeof(*tgtj));
	return;
}

static inline void
prep_pkt(udpc_seria_t sctx, job_t rplj, job_t srcj)
{
	copy_pkt(rplj, srcj);
	clear_pkt(sctx, rplj);
	return;
}

static inline void
send_pkt(udpc_seria_t sctx, job_t j)
{
	j->blen = UDPC_HDRLEN + udpc_seria_msglen(sctx);
	send_cl(j);
	UD_LOG("xdr-instr reply  "
	       ":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
	       (unsigned int)j->blen,
	       udpc_pkt_cno(JOB_PACKET(j)),
	       udpc_pkt_pno(JOB_PACKET(j)),
	       udpc_pkt_cmd(JOB_PACKET(j)),
	       ntohs(((const uint16_t*)j->buf)[3]));
	return;
}

#endif	/* INCLUDED_seria_proto_glue_h_ */
