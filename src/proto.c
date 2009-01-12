/*** proto.c -- unserding protocol
 *
 * Copyright (C) 2008 Sebastian Freundt
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* posix? */
#include <limits.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"

/***
 * The unserding protocol in detail:
 *
 * command: oi
 * replies: oi
 * purpose: announce yourself, can be used to fool the idle timer
 *
 * command: sup
 * replies: alright
 * purpose: return a list of cached queries
 *
 * command: cheers
 * replies: no worries
 * purpose: shows the unserding server how much you appreciated the
 *          last result set and makes him cache it for you
 *
 * command: wtf
 * replies: nvm
 * purpose: makes the unserding server forget about the last result
 *          set immediately
 *
 * command: ls
 * replies:
 * purpose: list the contents of the current catalogue
 *
 * command: spot
 * replies: spot
 * purpose: returns the spot price
 * example: spot :sym gfd/interests/eonia :asof today :cast ascii
 *
 ***/

static const char inv_rpl[] = "invalid command, better luck next time\n";

#define MAKE_SIMPLE_CMD(_c, _r)				\
	static const char ALGN16(_c##_cmd)[] = #_c;	\
	static const char ALGN16(_c##_rpl)[] = _r;

MAKE_SIMPLE_CMD(oi, "oi\n");
MAKE_SIMPLE_CMD(sup, "alright\n");
MAKE_SIMPLE_CMD(cheers, "no worries\n");
MAKE_SIMPLE_CMD(wtf, "nvm\n");
MAKE_SIMPLE_CMD(spot, "spot\n");
static const char ls_cmd[] = "ls";
static const char pwd_cmd[] = "pwd";
static const char cd_cmd[] = "cd";
static const char connect_cmd[] = "connect";
/* help command */
MAKE_SIMPLE_CMD(
	help,
	"oi     send keep-alive message\n"
	"sup    list connected clients and neighbour servers\n"
	"help   this help screen\n"
	"cheers [time]  store last result for TIME seconds (default 86400)\n"
	"wtf    immediately flush last result\n"
	"spot <options> obtain spot price\n"
	"ls     list catalogue entries of current directory\n"
	"pwd    print the name of the current directory\n"
	"cd X   change to directory X\n");

/* jobs */
static void ud_sup_job(job_t);
static void ud_spot_job(job_t);
/* helpers */
static void __spot_job(job_t);


/* string goodies */
/**
 * Find PAT (of length PLEN) inside BUF (of length BLEN). */
static const char __attribute__((unused)) *
boyer_moore(const char *buf, size_t blen, const char *pat, size_t plen)
{
	long int next[UCHAR_MAX];
	long int skip[UCHAR_MAX];

	if ((size_t)plen > blen || plen >= UCHAR_MAX) {
		return NULL;
	}

	/* calc skip table ("bad rule") */
	for (index_t i = 0; i <= UCHAR_MAX; i++) {
		skip[i] = plen;
	}
	for (index_t i = 0; i < plen; i++) {
		skip[(int)pat[i]] = plen - i - 1;
	}

	for (index_t j = 0, i; j <= plen; j++) {
		for (i = plen - 1; i >= 1; i--) {
			for (index_t k = 1; k <= j; k++) {
				if ((long int)i - (long int)k < 0L) {
					goto matched;
				}
				if (pat[plen - k] != pat[i - k]) {
					goto nexttry;
				}
			}
			goto matched;
		nexttry: ;
		}
	matched:
		next[j] = plen - i;
	}

	plen--;
	for (index_t i = plen /* position of last p letter */; i < blen; ) {
		for (index_t j = 0 /* matched letter count */; j <= plen; ) {
			if (buf[i - j] == pat[plen - j]) {
				j++;
				continue;
			}
			i += skip[(int)buf[i - j]] > next[j]
				? skip[(int)buf[i - j]]
				: next[j];
			goto newi;
		}
		return buf + i - plen;
	newi:
		;
	}
	return NULL;
}


/* the proto glot */
typedef uint8_t ud_msgfam_t;
typedef uint8_t ud_msgsvc_t;

/**
 * message families */
enum ud_msgfam_e {
	UD_UNKNOWN,
	UD_GENERIC,
	UD_DATARETR,

	NMSGFAMS
};

/**
 * the message header */
struct ud_msghdr_s {
	ud_msgfam_t fam;
	ud_msgsvc_t svc;
};

#define MAXHOSTNAMELEN		64
/* text section */
static char host[MAXHOSTNAMELEN];


void
ud_parse(job_t j)
{
/* clo is expected to be of type conn_ctx_t */
	UD_DEBUG_PROTO("parsing: \"%s\"\n", j->buf);

#define INNIT(_cmd)				\
	else if (memcmp(j->buf, _cmd, countof_m1(_cmd)) == 0 &&	\
		 ((j->buf[countof_m1(_cmd)] == '\n') ||		\
		  (j->buf[countof_m1(_cmd)] == ' ') ||		\
		  (j->buf[countof_m1(_cmd)] == '\0')))

	/* starting somewhat slowly with a memcmp */
	if (0) {
		;
	} INNIT(sup_cmd) {
		UD_DEBUG_PROTO("found `sup'\n");
		memcpy(j->buf, sup_rpl, j->blen = countof_m1(sup_rpl));
		j->prntf(j);

	} INNIT(oi_cmd) {
		size_t l;

		UD_DEBUG_PROTO("found `oi'\n");

		if (UNLIKELY(host[0] == '\0')) {
			(void)gethostname(host, countof(host));
		}
		memcpy(&j->buf[0], "oi ", 3);
		memcpy(&j->buf[3], host, countof(host));
		l = strlen(j->buf), j->buf[l] = '\n', j->blen = l + 1;
		UD_DEBUG_PROTO("constr \"%s\"\n", j->buf);
		j->prntf(j);

	} INNIT(cheers_cmd) {
		UD_DEBUG_PROTO("found `cheers'\n");
		memcpy(j->buf, cheers_rpl, j->blen = countof_m1(cheers_rpl));
		j->prntf(j);

	} INNIT(wtf_cmd) {
		UD_DEBUG_PROTO("found `wtf'\n");
		memcpy(j->buf, wtf_rpl, j->blen = countof_m1(wtf_rpl));
		j->prntf(j);

	} INNIT(spot_cmd) {
		UD_DEBUG_PROTO("found `spot'\n");
		__spot_job(j);

	} INNIT(ls_cmd) {
		job_t j2 = obtain_job(glob_jq);

		UD_DEBUG_PROTO("found `ls'\n");
		j2->prntf = j->prntf;
		j2->workf = ud_cat_ls_job;
		enqueue_job(glob_jq, j2);
		/* now notify the slaves */
		trigger_job_queue();

	} INNIT(help_cmd) {
		UD_DEBUG_PROTO("found `help'\n");
		memcpy(j->buf, help_rpl, j->blen = countof_m1(help_rpl));
		j->prntf(j);

	} else {
#if 0
		/* print an error */
		memcpy(j->buf, inv_rpl, j->blen = countof_m1(inv_rpl));
		j->prntf(j);
#endif
		j->blen = 0;
	}

#undef INNIT
#undef INNIT_CPL
	return;
}

static void __attribute__((unused))
ud_sup_job_cleanup(job_t j)
{
	return;
}

static void __attribute__((unused))
ud_sup_job(job_t j)
{
	return;
}


/* spot job */
#define DEFKEY(_x, _key)			\
	static const char _x[] = _key;

DEFKEY(_sym, ":sym");
DEFKEY(_asof, ":asof");
DEFKEY(_cast, ":cast");
DEFKEY(_from, ":from");
DEFKEY(_to, ":to");

typedef enum ud_keyw_e ud_keyw_t;
typedef const char *ud_sym_t;
typedef long unsigned int ud_date_t;
typedef long unsigned int ud_daterange_t;
typedef short unsigned int ud_type_t;

enum ud_keyw_e {
	K_UNKNOWN,
	K_SYM,
	K_ASOF,
	K_CAST,
	K_FROM,
	K_TO,
};

/** private */
struct spot_s {
	ud_sym_t sym;
	ud_date_t date;
	ud_daterange_t rng;
	ud_type_t ty;
};

static ud_keyw_t
parse_key(const char **buf)
{
/* parse key symbol in buf and put the value bit into next */
	if (UNLIKELY((*buf)[0] != ':')) {
		*buf = NULL;
		return K_UNKNOWN;
	}
	switch ((char)((*buf)[1])) {
	case 's':
		*buf += countof(_sym);
		return K_SYM;
	case 'a':
		*buf += countof(_asof);
		return K_ASOF;
	case 'c':
		*buf += countof(_cast);
		return K_CAST;
	case 'f':
		*buf += countof(_from);
		return K_FROM;
	case 't':
		*buf += countof(_to);
		return K_TO;
	default:
		*buf = NULL;
	}
	return K_UNKNOWN;
}

static ud_sym_t
parse_sym(const char **buf)
{
	const char *tmp = *buf;
	/* skip till ' ' and '\0' */
	while ((*tmp++ & ~0x20) != 0 && (*tmp++ & ~0x20) != 0);
	*buf = tmp;
	return (void*)0xdeadbeef;
}

static const char spot_synt[] =
	"Usage: spot :sym <symbol> "
	"[:asof <stamp> | :from <start> :to <end>] [:cast <type>]\n";

static void
__spot_job(job_t j)
{
	const char *cmd = j->buf + countof_m1(spot_cmd);
	struct spot_s res = {0, 0, 0, 0};

	if (UNLIKELY(cmd[0] == '\0')) {
		goto out;
	} else if (LIKELY(cmd[0] == ' ')) {
		cmd++;
	} else {
		goto out;
	}
	/* parse key/vals */
	do {
		switch (parse_key(&cmd)) {
		case K_SYM:
			res.sym = parse_sym(&cmd);
			break;
		case K_ASOF:
			res.date = 1210000000;
			break;
		case K_UNKNOWN:
		case K_CAST:
		case K_FROM:
		case K_TO:
		default:
			break;
		}
	} while (cmd && cmd[0] != '\0');

	if (LIKELY(res.sym != NULL)) {
		job_t j2 = obtain_job(glob_jq);
		j2->workf = ud_spot_job;
		j2->prntf = j->prntf;
		memcpy(j2->buf, &res, sizeof(res));
		enqueue_job(glob_jq, j2);
		/* now notify the slaves */
		trigger_job_queue();
	} else {
	out:
		memcpy(j->buf, spot_synt, j->blen = countof_m1(spot_synt));
		j->prntf(j);
	}
	return;
}


/* alibi definition here, will be sodded */
static void
ud_spot_job(job_t j)
{
	memcpy(j->buf, "2.52\n", j->blen = 5);
	j->prntf(j);
	return;
}

/* proto.c ends here */
