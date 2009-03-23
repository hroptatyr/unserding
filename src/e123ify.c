/*** e123ify.c -- example client to reformat phone numbers compliant to E.123
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

#include "config.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "unserding.h"
#include "protocore.h"

#define TIMEOUT		100 /* milliseconds */

static bool
allowed_char_p(char c)
{
	/* likely chars first */
	switch (c) {
	case '0' ... '9':
	case '+':
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '/':
		return true;

	default:
		break;
	}
	return false;
}

static int
strip_number(char *restrict buf, const char *number)
{
	int len = 0;

	/* trivial cases first */
	if (number == NULL) {
		return 0;
	}

	/* strip stuff */
	for (int i = 0; number[i] != '\0'; i++) {
		/* only copy allowed characters */
		if (allowed_char_p(number[i])) {
			buf[len++] = number[i];
		}
	}
	return len;
}

static bool
usefulp(ud_packet_t pkt, void *closure)
{
	ud_convo_t cno = (ud_convo_t)(long unsigned int)closure;

	if (udpc_pkt_cno(pkt) != cno) {
		/* it's not our convo */
		return false;
	}
	if (pkt.pbuf[8] != UDPC_TYPE_STRING || pkt.pbuf[9] == 0x00) {
		/* answer is empty */
		return false;
	}
	return true;
}


/* sender */
static ud_convo_t
e123ify_send(ud_handle_t hdl, const char *number)
{
	char buf[UDPC_SIMPLE_PKTLEN];
	char *restrict work_space;
	ud_packet_t pkt = {sizeof(buf), buf};
	ud_convo_t cno;

	/* set up the packet, should be a fun? */
	memset(buf, 0, sizeof(buf));
	cno = ud_handle_convo(hdl);
	udpc_make_pkt(pkt, cno, /*pno*/0, UDPC_PKT_E123);

	/* add their args, the length is taken from the stripper */
	pkt.pbuf[8] = UDPC_TYPE_STRING;
	work_space = &pkt.pbuf[10];
	pkt.pbuf[9] = strip_number(work_space, number);

	/* set packet size and fill in any remaining args */
	pkt.plen = 8 + 2 + (pkt.pbuf[9] = strlen(work_space));
	/* send him */
	ud_send_raw(hdl, pkt);
	hdl->convo++;
	return cno;
}

static int
e123ify_recv(ud_handle_t hdl, ud_convo_t cno)
{
	char buf[UDPC_SIMPLE_PKTLEN];
	char *work_space;
	ud_packet_t pkt = {sizeof(buf), buf};
	void *clo = (void*)(long unsigned int)cno;

	/* clean the packet space */
	memset(pkt.pbuf, 0, UDPC_SIMPLE_PKTLEN);
	ud_recv_pred(hdl, &pkt, TIMEOUT, usefulp, clo);

	if (!usefulp(pkt, clo)) {
		fprintf(stderr, "No answers\n");
		return 1;
	}
	/* prepare the buffer for string output */
	work_space = &pkt.pbuf[10];
	work_space[pkt.pbuf[9]] = '\0';
	/* out him */
	fputs(work_space, stdout);
	fputc('\n', stdout);
	return 0;
}


/* modes */
static void
cli_mode(ud_handle_t hdl, int argc, const char *argv[])
{
	ud_convo_t cno;

	for (int i = 1; i < argc; i++) {
		/* query the bugger */
		cno = e123ify_send(hdl, argv[i]);
		/* receive */
		(void)e123ify_recv(hdl, cno);
	}
	return;
}

static void
stdio_mode(ud_handle_t hdl)
{
	ssize_t nread;
	size_t llen = 256;
	ud_convo_t cno;
	char *lbuf;

	/* get a line buffer */
	lbuf = malloc(llen);
	/* process line by line */
	while ((nread = getline(&lbuf, &llen, stdin)) > 0) {
		/* query the bugger */
		cno = e123ify_send(hdl, lbuf);
		/* receive */
		(void)e123ify_recv(hdl, cno);
	}
	/* free the line buffer */
	free(lbuf);
	return;
}


int
main(int argc, const char *argv[])
{
	struct ud_handle_s __hdl;

	/* obtain us a new handle */
	init_unserding_handle(&__hdl);

	if (argc > 1) {
		cli_mode(&__hdl, argc, argv);
	} else {
		stdio_mode(&__hdl);
	}

	/* free the handle */
	free_unserding_handle(&__hdl);
	return 0;
}

/* e123ify.c ends here */
