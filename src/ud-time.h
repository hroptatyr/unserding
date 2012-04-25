/*** ud-time.h -- time and clock goodness
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

#if !defined INCLUDED_ud_time_h_
#define INCLUDED_ud_time_h_

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
/* for gettimeofday() */
#include <sys/time.h>
/* for struct timespec */
#include <time.h>

#if defined __USE_POSIX199309 && !defined UNSERLIB
/* returns the current CLOCK_REALTIME time stamp */
static inline struct timespec
__stamp(void)
{
	struct timespec res;
	clock_gettime(CLOCK_REALTIME, &res);
	return res;
}

/* given a stamp THEN, returns the difference between __stamp() and THEN. */
static inline struct timespec
__lapse(struct timespec then)
{
	struct timespec now = __stamp();
	struct timespec res;

	res.tv_sec = now.tv_sec - then.tv_sec;
	res.tv_nsec = now.tv_nsec - then.tv_nsec;
	/* deal with overflow */
	if (now.tv_nsec < then.tv_nsec) {
		res.tv_sec--;
		res.tv_nsec += 1000000000U;
	}
	return res;
}

static inline double
__as_f(struct timespec src)
{
/* return time as float in milliseconds */
	return src.tv_sec * 1000.f + src.tv_nsec / 1000000.f;
}
#endif	/* __USE_POSIX199309 && !UNSERLIB */

static inline struct timeval
__ustamp(void)
{
	struct timeval res;
	gettimeofday(&res, NULL);
	return res;
}

/* given a stamp THEN, return the difference between __ustamp() and THEN. */
static inline struct timeval
__ulapse(struct timeval then)
{
	struct timeval now = __ustamp();
	struct timeval res;

	res.tv_sec = now.tv_sec - then.tv_sec;
	res.tv_usec = now.tv_usec - then.tv_usec;
	/* deal with overflow */
	if (now.tv_usec < then.tv_usec) {
		res.tv_sec--;
		res.tv_usec += 1000000U;
	}
	return res;
}

static inline double
__uas_f(struct timeval src)
{
/* return time as float in milliseconds */
	return src.tv_sec * 1000.f + src.tv_usec / 1000.f;
}

/* printers */
static inline size_t
print_ts_into(char *restrict tgt, size_t len, time_t ts)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	(void)gmtime_r(&ts, &tm);
	return strftime(tgt, len, "%Y-%m-%d %H:%M:%S", &tm);
}

static inline size_t
print_ds_into(char *restrict tgt, size_t len, time_t ts)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	(void)gmtime_r(&ts, &tm);
	return strftime(tgt, len, "%Y-%m-%d", &tm);
}

/* pfack's __daydiff() and __dayofweek() but over time32_t */
/* date/time */
#if !defined time32_t
typedef int32_t time32_t;
#define time32_t	time32_t
#endif	/* !time32_t */

typedef enum dow_e dow_t;

enum dow_e {
	DOW_SUNDAY,
	DOW_MONDAY,
	DOW_TUESDAY,
	DOW_WEDNESDAY,
	DOW_THURSDAY,
	DOW_FRIDAY,
	DOW_SATURDAY,
	DOW_MIRACLEDAY,
};

static inline time32_t __attribute__((always_inline))
__midnight(time32_t ts)
{
	return ts - ts % 86400U;
}

static inline int __attribute__((always_inline))
__daydiff(time32_t t1, time32_t t2)
{
	time32_t m1 = __midnight(t1);
	time32_t m2 = __midnight(t2);
	return (m2 - m1) / 86400U;
}

static inline dow_t __attribute__((always_inline))
__dayofweek(time32_t t)
{
	/* we know that 15/01/1984 was a sunday, and this is 442972800 */
	t = __daydiff((time32_t)442972800, t);
	return (dow_t)(t % 7);
}

#endif	/* INCLUDED_ud_time_h_ */
