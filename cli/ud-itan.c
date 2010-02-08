/*** ud-itan.c -- convenience tool to obtain itans without a hastle
 *
 * Copyright (C) 2010 Sebastian Freundt
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

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include <errno.h>
#include "svc-itanl.h"


#include "btea.c"
/* key fiddling */
static uint32_t gkey[4] = {0xdeadbeef, 0xdeadbeef, 0xcafebabe, 0xcafebabe};

static void
genkey_from_pass(void)
{
	char *pw = getpass("Passphrase: "), *p, *k;
	int ps = strlen(pw);
	/* scatter, pw over k */
	for (p = pw, k = (char*)gkey; k < (char*)gkey + sizeof(gkey); ) {
		*k++ ^= *p++;
		if (p - pw >= ps) {
			p = pw;
		}
	}
	free(pw);

	/* encrypt the shite */
	btea_enc(gkey, sizeof(gkey) / sizeof(uint32_t), gkey);
	return;
}


static struct ud_handle_s hdl[1];

static bool
cb(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(s), void *UNUSED(c))
{
	struct udpc_seria_s sctx[1];
	const char *res = NULL;

	udpc_seria_init(sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	if (udpc_seria_des_str(sctx, &res) > 0) {
		printf("%s\n", res);
	}
	return false;
}

static void
add_tan(udpc_seria_t sctx, const char *tan, size_t len)
{
	char tmp[8];

	memcpy(tmp, tan, len > 8 ? 8 : len);
	btea_enc((void*)tmp, sizeof(tmp) / sizeof(uint32_t), gkey);
	udpc_seria_add_str(sctx, tmp, sizeof(tmp));
	return;
}


int
main(int argc, const char *argv[])
{
	/* ud nonsense */
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.pbuf = buf, .plen = sizeof(buf)};
	struct udpc_seria_s sctx[1];
	long int idx;
	const char *tan = NULL;

	if (argc <= 1 || argc > 3) {
		fprintf(stderr, "Usage: ud-itan index [tan]\n");
		exit(1);
	}

	/* query mode */
	idx = strtol(argv[1], NULL, 0);
	if (argc == 3) {
		/* set mode */
		tan = argv[2];
	}
	/* before we start any complicated games, read the passphrase */
	genkey_from_pass();
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* now kick off the finder */
	udpc_make_pkt(pkt, 0, 0, UD_SVC_ITANL);
	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* ts first */
	udpc_seria_add_ui16(sctx, (uint16_t)idx);
	if (tan) {
		add_tan(sctx, tan, strlen(tan));
	}
	pkt.plen = udpc_seria_msglen(sctx) + UDPC_HDRLEN;

	/* send the packet */
	ud_send_raw(hdl, pkt);
	if (tan == NULL) {
		/* ... and receive the answers, only in query mode */
		ud_subscr_raw(hdl, 3000, cb, NULL);
	}
	/* and lose the handle again */
	free_unserding_handle(hdl);
	return 0;
}

/* ud-itan.c ends here */
