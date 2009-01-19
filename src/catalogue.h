/*** unserding.c -- unserding network service
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

#if !defined INCLUDED_catalogue_h_
#define INCLUDED_catalogue_h_

#include "unserding.h"
#include <stdarg.h>

/**
 * The catalogue data structure.
 * Naive. */
struct ud_cat_s {
	ud_cat_t next;
	ud_catobj_t data;
};

/**
 * The catalogue object data structure.
 * Naive. */
struct ud_catobj_s {
	uint8_t nattrs;
	ud_tlv_t attrs[];
};

/**
 * A tag-length-value cell.  The length bit is either implicit or coded
 * into data somehow. */
struct ud_tlv_s {
	ud_tag_t tag;
	const char data[];
};

/* navigatable tlv cell */
struct ud_tlvcons_s {
	ud_tlvcons_t next;
	ud_tlv_t tlv;
};

/* tlv and tlvcons goodies */
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_tlv(ud_tag_t tag, uint8_t size);
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_tlv(ud_tag_t tag, uint8_t size)
{
	ud_tlv_t res = (void*)malloc(sizeof(ud_tag_t) + size);
	res->tag = tag;
	return res;
}

extern inline void __attribute__((always_inline, gnu_inline))
free_tlv(ud_tlv_t tlv);
extern inline void __attribute__((always_inline, gnu_inline))
free_tlv(ud_tlv_t tlv)
{
	free(tlv);
	return;
}

extern inline ud_tlvcons_t __attribute__((always_inline, gnu_inline))
make_tlvcons(ud_tlv_t tlv);
extern inline ud_tlvcons_t __attribute__((always_inline, gnu_inline))
make_tlvcons(ud_tlv_t tlv)
{
	ud_tlvcons_t res = (void*)malloc(sizeof(struct ud_tlvcons_s));
	res->tlv = tlv;
	return res;
}

extern inline void __attribute__((always_inline, gnu_inline))
free_tlvcons(ud_tlvcons_t tlv);
extern inline void __attribute__((always_inline, gnu_inline))
free_tlvcons(ud_tlvcons_t tlv)
{
	free(tlv);
	return;
}

/* special tlv cells */
extern inline ud_catobj_t __attribute__((always_inline, gnu_inline))
ud_make_catobj(ud_tlv_t o, ...);
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_class(const char *cls, uint8_t size);
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_name(const char *nam, uint8_t size);

extern void __ud_fill_catobj(ud_catobj_t co, ...);

extern inline ud_catobj_t __attribute__((always_inline, gnu_inline))
ud_make_catobj(ud_tlv_t o, ...)
{
	uint8_t n = 1 + __builtin_va_arg_pack_len();
	ud_catobj_t res = (void*)malloc(
		offsetof(struct ud_catobj_s, attrs) + n * sizeof(ud_tlv_t));

	res->nattrs = n;
	__ud_fill_catobj(res, o, __builtin_va_arg_pack());
	return res;
}

/* special tlv cells */
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_class(const char *cls, uint8_t size)
{
	ud_tlv_t res = make_tlv(UD_TAG_CLASS, size+1);
	(((char*)res) + offsetof(struct ud_tlv_s, data))[0] = (char)size;
	memcpy((((char*)res) + offsetof(struct ud_tlv_s, data)) + 1, cls, size);
	return res;
}

extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
make_name(const char *nam, uint8_t size)
{
	ud_tlv_t res = make_tlv(UD_TAG_NAME, size+1);
	(((char*)res) + offsetof(struct ud_tlv_s, data))[0] = (char)size;
	memcpy((((char*)res) + offsetof(struct ud_tlv_s, data)) + 1, nam, size);
	return res;
}

#define MAKE_CLASS(_x)	make_class(_x, countof_m1(_x))
#define MAKE_NAME(_x)	make_name(_x, countof_m1(_x))

#endif	/* INCLUDED_catalogue_h_ */
