/*** ud-instr.c -- fetch instruments and time series intervals
 *
 * Copyright (C) 2005 - 2009 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdbool.h>
#include <popt.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "ud-time.h"
#include "tscoll.h"

static bool xmlp;
static bool tslabp;


/* porno mode to behold the packet loss */
struct pcb_clo_s {
	size_t cnt;
	ud_pkt_no_t pno;
};

static bool
pcb(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
{
	struct pcb_clo_s *pcb_clo = clo;
	struct udpc_seria_s sctx;
	const void *foo;
	ud_pkt_no_t pno;
	size_t len;

	if (UDPC_PKT_INVALID_P(pkt)) {
		fprintf(stderr, "that's it\n");
		return false;
	}
	if ((pno = udpc_pkt_pno(pkt)) > pcb_clo->pno + 1) {
		for (unsigned int i = pcb_clo->pno + 1; i < pno; i++) {
			fprintf(stderr, "pkt %u went missing\n", i);
		}
	}
	pcb_clo->pno = pno;

	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	if ((len = udpc_seria_des_xdr(&sctx, &foo)) > 0) {
		XDR hdl;
		struct instr_s ins;
		init_instr(&ins);
		xdrmem_create(&hdl, (caddr_t)&foo, len, XDR_DECODE);
		while (true) {
			if (!xdr_instr_s(&hdl, &ins)) {
				break;
			}
			pcb_clo->cnt++;
		}
		xdr_destroy(&hdl);
	} else {
		fprintf(stderr, "uh oh, packet looks cunted\n");
	}
	/* more packets please */
	return true;
}

static void
find_one_instr(ud_handle_t hdl, const char *uii)
{
	uint32_t cid;
	const void *data;
	size_t len;
	struct instr_s in[1];

	if ((cid = strtol(uii, NULL, 10)) == 0) {
		len = ud_find_one_isym(hdl, &data, uii, strlen(uii));
	} else {
		len = ud_find_one_instr(hdl, &data, cid);
	}
	if (len > 0) {
		/* data here points to an xdr-encoded instr */
		deser_instrument_into(in, data, len);
		print_instr(stdout, in);
		fputc('\n', stdout);
	} else {
		fprintf(stdout, "%s unknown\n", uii);
		return;
	}
	/* now use the gaii of the instr(s) fetched */
	cid = instr_gaid(in);
	if ((len = ud_find_one_tslab(hdl, &data, cid)) > 0) {
		/* data hereby points to a tseries object */
		const struct tslab_s *s = data;
		char los[32], his[32];
		/* debugging mumbo jumbo */
		print_ts_into(los, sizeof(los), s->from);
		print_ts_into(his, sizeof(his), s->till);
		fprintf(stdout, "  tslab %u/%u@%hu %u %s..%s\n",
			su_secu_quodi(s->secu),
			su_secu_quoti(s->secu),
			su_secu_pot(s->secu),
			s->types, los, his);
	} else {
		fputs("  no tslabs yet\n", stdout);
	}
}

static void
porno_mode(ud_handle_t hdl)
{
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct pcb_clo_s pcb_clo = {.cnt = 0, .pno = 0};

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_INSTR_BY_ATTR);
	/* prepare packet for sending im off */
	pkt.plen = UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);
	ud_subscr_raw(hdl, 1000, pcb, &pcb_clo);
	fprintf(stdout, "recv'd %zu instrs\n", pcb_clo.cnt);
	return;
}

static void
find_them_instrs(ud_handle_t hdl, const char *const *insv)
{
	if (insv == NULL) {
		/* no uii's at all */
		porno_mode(hdl);
		return;
	}
	/* read the UII's from the command line */
	for (const char *const *uii = insv; *uii; uii++) {
		find_one_instr(hdl, *uii);
	}
	return;
}


/* option magic */
typedef enum poptCallbackReason __attribute__((unused)) cbr_t;
typedef poptContext c_t;

static void
help_cb(
	poptContext con, enum poptCallbackReason UNUSED(foo),
	struct poptOption *key, UNUSED(const char *arg), UNUSED(void *data))
{
	if (key->shortName == 'h') {
		poptPrintHelp(con, stdout, 0);
	} else if (key->shortName == 'V') {
		fprintf(stdout, "ud-instr " PACKAGE_VERSION "\n");
	} else {
		poptPrintUsage(con, stdout, 0);
	}
	exit(0);
	return;
}

static struct poptOption out_opts[] = {
	{"xml", 'x', POPT_ARG_NONE,
	 &xmlp, 0,
	 "Output instrument specs in XML.", NULL},
	{"tslab", 't', POPT_ARG_NONE,
	 &tslabp, 0,
	 "Request tick slab for each instrument.", NULL},
	POPT_TABLEEND
};

static struct poptOption help_opts[] = {
	{NULL, '\0', POPT_ARG_CALLBACK, (void*)help_cb, 0, NULL, NULL},
	{"help", 'h', 0, NULL, '?', "Show this help message", NULL},
	{"version", 'V', 0, NULL, 'V', "Print version string and exit.", NULL},
	{"usage", '\0', 0, NULL, 'u', "Display brief usage message", NULL},
	POPT_TABLEEND
};

static struct poptOption porn_opts[] = {
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, out_opts, 0,
	 "Output options", NULL},
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, help_opts, 0,
	 "Help options", NULL},
        POPT_TABLEEND
};

static const char *const*
ud_parse_cl(size_t argc, const char *argv[])
{
        poptContext opt_ctx;

        opt_ctx = poptGetContext(NULL, argc, argv, porn_opts, 0);
        poptSetOtherOptionHelp(
		opt_ctx,
		" "
		"uii [uii [...]]");

        /* auto-do */
        while (poptGetNextOpt(opt_ctx) > 0) {
                /* Read all the options ... */
                ;
        }
        return poptGetArgs(opt_ctx);
}


int
main(int argc, const char *argv[])
{
	struct ud_handle_s __hdl;
	ud_handle_t hdl = &__hdl;
	const char *const *rest;

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* now kick off the finder */
	find_them_instrs(hdl, rest);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-instr.c ends here */
