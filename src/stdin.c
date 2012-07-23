/*** stdin.c -- network and console handlers for the unserding client
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
#include <string.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_NETDB_H
# include <netdb.h>
#elif defined HAVE_LWRES_NETDB_H
# include <lwres/netdb.h>
#endif	/* NETDB */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
/* posix */
#include <pwd.h>
#define USE_READLINE	1
#if USE_READLINE
/* gnu readline */
# include <readline/readline.h>
# include <readline/history.h>
#else
/* bsd's editline */
# include <editline/readline.h>
# include <histedit.h>
#endif	/* USE_READLINE */
/* our master file, includes the libev event loop */
#include "stdin.h"
/* for LIKELY et al. */
#include "unserding-nifty.h"
#include "unserding-dbg.h"

static int lsock __attribute__((used));
static ev_io __srv_watcher __attribute__((aligned(16)));

#define HISTFILE	".unsercli_history"
static char histfile[256];

static void
__handle_cb(const char *UNUSED(line), size_t UNUSED(len))
{
	return;
}
static void (*handle_cb)(const char *line, size_t len) = &__handle_cb;


/* string goodies */
static void
handle_el(char *line)
{
	/* print newline */
	if (UNLIKELY(line == NULL)) {
		/* print newline */
		putc('\n', stdout);
		/* just remove the handler here */
		rl_callback_handler_remove();
		/* and finally kick the event loop */
		ev_unloop(EV_DEFAULT_ EVUNLOOP_ALL);
		return;
	}
	UD_DEBUG_STDIN("received line \"%s\"\n", line);
	/* save us some work */
	if (UNLIKELY(line[0] == '\0' || line[0] == ' ')) {
		return;
	}
	/* stuff up our history */
	add_history(line);

	/* parse him, blocks until a reply is nigh */
	handle_cb(line, rl_end);

	/* and free him */
	free(line);
	return;
}

/* dirty */
void
stdin_print_async(const char *buf, size_t UNUSED(len))
{
	/* clear the current bugger */
	rl_crlf();
	/* plug ourselves */
	fputs(buf, stdout);
	/* hm, let's hope they put a newline last */
	rl_redisplay();
	return;
}

/* completers */
/* Attempt to complete on the contents of TEXT.  START and END
   bound the region of rl_line_buffer that contains the word to
   complete.  TEXT is the word to complete.  We can use the entire
   contents of rl_line_buffer in case we want to do some simple
   parsing.  Return the array of matches, or NULL if there aren't any. */
static char**
udcli_comp(const char *text, int start, int UNUSED(end))
{
	char **matches;

	matches = (char**)NULL;

	/* If this word is at the start of the line, then it is a command
	   to complete.  Otherwise it is the name of a file in the current
	   directory. */
	if (start == 0) {
		/* TODO */
		matches = rl_completion_matches(text, cmd_generator);
	}
	return matches;
}


#if !USE_READLINE && 0
static void *hist;
static void *el;
static HistEvent histev;

static char*
prompt(void *p)
{
	return "unserding> ";
}
#endif

static int
stdin_listener_init(void)
{
	/* initialise the readline */
	rl_initialize();
	rl_readline_name = "unserding";
	rl_attempted_completion_function = udcli_comp;

	rl_basic_word_break_characters = "\t\n@$><=;|&{( ";
#if USE_READLINE
	rl_catch_signals = 0;
#endif

	/* the callback */
	rl_callback_handler_install("unserding> ", handle_el);

	/* load the history file */
	(void)read_history(histfile);
	history_set_pos(history_length);

#if 0  /* !USE_READLINE */
	/* load the history file */
	hist = history_init();
	history(hist, &histev, H_LOAD, histfile);
	/* tell editline to use this history */
	el_set(el, EL_HIST, history, hist);
#endif	/* USE_READLINE */

	/* succeeded if == 0 */
	return STDIN_FILENO;
}

static void
stdin_listener_deinit(EV_P_ int sock)
{
	UD_DEBUG_STDIN("deinitialising readline\n");
	rl_callback_handler_remove();

	/* save the history file */
	(void)write_history(histfile);

#if 0  /* !USE_READLINE */
	history(hist, &histev, H_SAVE, histfile);
	history_end(hist);
#endif	/* USE_READLINE */

	UD_DEBUG_STDIN("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}

/* this callback is called when data is readable on one of the polled socks */
static void
stdin_traf_rcb(EV_P_ ev_io *UNUSED(w), int UNUSED(revents))
{
	rl_callback_read_char();
	return;
}

static void
init_histfile()
{
	char pwdbuf[256], *p = histfile;
        struct passwd pwdstr;
        struct passwd *pwd = NULL;

        if (getpwuid_r(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd) != 0) {
                return;
	}
        p += snprintf(histfile, countof(histfile), "%s", pwd->pw_dir);
	*p++ = '/';
	memcpy(p, HISTFILE, countof(HISTFILE));
        return;
}


void
ud_reset_stdin(EV_P)
{
	rl_delete_text(0, rl_end);
	putc('\n', stdout);
#if USE_READLINE
	rl_on_new_line();
#endif
	rl_forced_update_display();
	return;
}

int
ud_attach_stdin(EV_P_ void(*hcb)(const char*, size_t))
{
	ev_io *srv_watcher = &__srv_watcher;

	/* initialise the histfile */
	init_histfile();
	/* get us a global sock */
	lsock = stdin_listener_init();

	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, stdin_traf_rcb, lsock, EV_READ);
	ev_io_start(EV_A_ srv_watcher);

	/* set the handler callback */
	handle_cb = hcb;
	return 0;
}

int
ud_detach_stdin(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;

	/* reset the handler callback */
	handle_cb = &__handle_cb;

	/* close the socket et al */
	stdin_listener_deinit(EV_A_ lsock);

	/* stop the io watcher */
	ev_io_stop(EV_A_ srv_watcher);
	return 0;
}

/* stdin.c ends here */
