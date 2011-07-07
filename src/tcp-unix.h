/*** tcp-unix.c -- tcp/unix handlers
 *
 * Copyright (C) 2011 Sebastian Freundt
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

#if !defined INCLUDED_tcp_unix_h_
#define INCLUDED_tcp_unix_h_

#include <sys/types.h>
#include <sys/stat.h>

typedef struct __conn_s *ud_conn_t;

typedef int(*ud_ccb_f)(ud_conn_t, void*);
typedef int(*ud_cicb_f)(ud_conn_t, const char *f, const struct stat*, void*);
typedef int(*ud_cbcb_f)(ud_conn_t, char *buf, size_t len, void*);

/* ctors/dtors */
/**
 * Make a tcp listener, bind it to PORT.
 * Connections are accepted automatically, the DATA_IN callback is called
 * when new data has been read, the CLO callback is called when the
 * connection is closed. */
extern ud_conn_t
make_tcp_conn(uint16_t port, ud_cbcb_f data_in, ud_ccb_f clo, void *data);
/**
 * Make a unix listener, bind it to PATH.
 * Connections are accepted automatically, the DATA_IN callback is called
 * when new data has been read, the CLO callback is called when the
 * connection is closed. */
extern ud_conn_t
make_unix_conn(const char *path, ud_cbcb_f data_in, ud_ccb_f clo, void *data);
/**
 * Make an inotify listener, operate on FILE.
 * Whenever FILE changes the callback INOT_CB is called. */
extern ud_conn_t
make_inot_conn(const char *file, ud_cicb_f inot, void *data);
/**
 * Close the listener CONN and return its data. */
extern void *ud_conn_fini(ud_conn_t c);

extern void *ud_conn_get_data(ud_conn_t ctx);
extern void ud_conn_put_data(ud_conn_t ctx, void *data);


/* helper functions for as long as there is no edge-triggered writer
 * CB is called when the buffer has been written completely or there
 * was an error. */
extern ud_conn_t
ud_write_soon(ud_conn_t, const char *buf, size_t len, ud_cbcb_f eo_wri);

#endif	/* INCLUDED_tcp_unix_h_ */
