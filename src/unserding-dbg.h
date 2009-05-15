/*** unserding-dbg.h -- debugging and logging mumbo jumbo
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

#if !defined INCLUDED_unserding_dbg_h_
#define INCLUDED_unserding_dbg_h_

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#if defined HAVE_PTHREAD_H
# include <pthread.h>
#endif
#if defined HAVE_STRING_H
# include <string.h>
#endif
#if defined HAVE_STDINT_H
# include <stdint.h>
#endif
#if defined HAVE_STDARG_H
# include <stdarg.h>
#endif
#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif


/* logging */
extern FILE *logout;

#if defined UNSERSRV && defined DEBUG_FLAG
#define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)			\
	fprintf(logout, "[unserding] " args)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(logout, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(logout, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)				\
	fprintf(logout, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)			\
	fprintf(logout, "[unserding/stdin] " args)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(logout, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)			\
	fprintf(logout, "[unserding/proto] " args)
# define UD_CRITICAL_CAT(args...)				\
	fprintf(logout, "[unserding/catalogue] CRITICAL " args)
# define UD_DEBUG_CAT(args...)				\
	fprintf(logout, "[unserding/catalogue] " args)
# define UD_DBGCONT(args...)			\
	fprintf(logout, args)

#elif defined UNSERCLI && defined DEBUG_FLAG
# define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(logout, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(logout, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)			\
	fprintf(logout, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#elif defined UNSERMON && defined DEBUG_FLAG
# define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(logout, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#else  /* aux stuff */
# define UD_CRITICAL(args...)			\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)
#endif

#if defined UNSERMON
# define UD_UNSERMON_PKT(args...)	fprintf(logout, "%02x:%06x: " args)
#endif	/* UNSERMON */


#if defined UNSERSRV
# if defined TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else  /* !TIME_WITH_SYS_TIME */
#  if defined HAVE_SYS_TIME_H
#   include <sys/time.h>
#  endif
#  if defined HAVE_TIME_H
#   include <time.h>
#  endif
# endif

extern inline void __attribute__((always_inline, gnu_inline,format(printf,1,0)))
__ud_log(const char *restrict fmt, ...);
# define UD_LOG(args...)	__ud_log("%lu.%09u [unserding] " args)
# define UD_LOG_MCAST(args...)	__ud_log("%lu.%09u [unserding/mcast] " args)

# if __GNUC_PREREQ(4,3)
/* methinks this is fuck ugly, but kinda cool */
extern inline void __attribute__((always_inline, gnu_inline,format(printf,1,0)))
__ud_log(const char *restrict fmt, ...)
{
#  if defined HAVE_CLOCK_GETTIME || 1 /* check for me */
	struct timespec n;
#  else	 /* !HAVE_CLOCK_GETTIME */
#   error "WTF?!"
#  endif  /* HAVE_CLOCK_GETTIME */
	clock_gettime(CLOCK_REALTIME, &n);
	/* there must be %lu.%09u in the format string */
	fprintf(logout, fmt, n.tv_sec, n.tv_nsec, __builtin_va_arg_pack());
	fflush(logout);
	return;
}
# else	/* !4.3 */
#  error "Use gcc 4.3 or give me va_arg_pack() implementation!"
# endif	 /* >= 4.3 */
#else
# define UD_LOG(args...)
# define UD_LOG_MCAST(args...)
#endif	/* UNSERSRV */

#endif	/* INCLUDED_unserding_dbg_h_ */
