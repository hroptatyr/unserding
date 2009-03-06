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
#include <stddef.h>
#if defined UNSERSRV
# include <pfack/instruments.h>
#endif	/* UNSERSRV */

/**
 * The catalogue data type, just a husk. */
typedef void *catng_t;

#define UD_TAG_GROUP0(_x)	((_x) + 0x20)
#define UD_TAG_GROUP1(_x)	((_x) + 0x30)
#define UD_TAG_GROUP2(_x)	((_x) + 0x40)
#define UD_TAG_GROUP3(_x)	((_x) + 0x50)
#define UD_TAG_GROUP4(_x)	((_x) + 0x60)
#define UD_TAG_GROUP5(_x)	((_x) + 0x70)

/* tags */
enum ud_tag_e {
	UD_TAG_UNK,
	/* :class takes UDPC_TYPE_STRING */
	UD_TAG_CLASS,
	/* :attr / :attribute takes UDPC_TYPE_ATTR
	 * this indicates which operations can be used on an entry
	 * attribute spot to obtain spot values
	 * attribute trad to obtain trade values
	 * etc. */
	UD_TAG_ATTR,
	/* :name takes UDPC_TYPE_STRING */
	UD_TAG_NAME,
	/* :date takes UDPC_TYPE_DATE* (one of the date types) */
	UD_TAG_DATE,
	/* :expiry ... dunno bout that one, behaves like :date */
	UD_TAG_EXPIRY,
	/* :paddr takes UDPC_TYPE_POINTER, just a 64bit address really */
	UD_TAG_PADDR,
	/* :underlying ... dunno bout that one, behaves like :paddr */
	UD_TAG_UNDERLYING,
	/* :price takes UDPC_TYPE_MON* or UDPC_TYPE_*FLOAT */
	UD_TAG_PRICE,
	/* :strike ... dunno, it's like :price really */
	UD_TAG_STRIKE,
	/* :place ... takes UDPC_TYPE_STRING */
	UD_TAG_PLACE,
	/* :symbol takes UDPC_TYPE_STRING */
	UD_TAG_SYMBOL,
	/* :currency takes UDPC_TYPE_STRING */
	UD_TAG_CURRENCY,

	/* :group0 */
	UD_TAG_GROUP0_NAME = UD_TAG_GROUP0(0),
	UD_TAG_GROUP0_CFI = UD_TAG_GROUP0(1),
	UD_TAG_GROUP0_OPOL = UD_TAG_GROUP0(2),
	UD_TAG_GROUP0_GAID = UD_TAG_GROUP0(3),

	UD_TAG_GROUP1_ISIN = UD_TAG_GROUP1(0),
	UD_TAG_GROUP1_RIC = UD_TAG_GROUP1(1),
	UD_TAG_GROUP1_BBG = UD_TAG_GROUP1(2),

	UD_TAG_GROUP2_FUND_INSTR = UD_TAG_GROUP2(0),
	UD_TAG_GROUP2_SETD_INSTR = UD_TAG_GROUP2(1),

	UD_TAG_GROUP3_ISSUE = UD_TAG_GROUP3(0),
	UD_TAG_GROUP3_EXPIRY = UD_TAG_GROUP3(1),
	UD_TAG_GROUP3_SETTLE = UD_TAG_GROUP3(2),

	UD_TAG_GROUP4_UNDERLYER = UD_TAG_GROUP4(0),
	UD_TAG_GROUP4_STRIKE = UD_TAG_GROUP4(1),
	UD_TAG_GROUP4_RATIO = UD_TAG_GROUP4(2),

	UD_TAG_GROUP5_BARRIER = UD_TAG_GROUP5(0),

	/* first allowed tag for instrument filters */
	UD_TAG_INSTRFILT_FIRST = UD_TAG_GROUP0_NAME,
	/* last allowed tag for instrument filters */
	UD_TAG_INSTRFILT_LAST = UD_TAG_GROUP5_BARRIER,
};

/**
 * Structure for gperf htables */
struct ud_tagtbl_s {
	const char *name;
	ud_tag_t tag;
};

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
ud_make_class(const char *cls, uint8_t size);
extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
ud_make_name(const char *nam, uint8_t size);

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
ud_make_class(const char *cls, uint8_t size)
{
	ud_tlv_t res = make_tlv(UD_TAG_CLASS, size+1);
	(((char*)res) + offsetof(struct ud_tlv_s, data))[0] = (char)size;
	__builtin_memcpy(
		(((char*)res) + offsetof(struct ud_tlv_s, data))+1, cls, size);
	return res;
}

extern inline ud_tlv_t __attribute__((always_inline, gnu_inline))
ud_make_name(const char *nam, uint8_t size)
{
	ud_tlv_t res = make_tlv(UD_TAG_NAME, size+1);
	(((char*)res) + offsetof(struct ud_tlv_s, data))[0] = (char)size;
	__builtin_memcpy(
		(((char*)res) + offsetof(struct ud_tlv_s, data))+1, nam, size);
	return res;
}

#define UD_MAKE_CLASS(_x)	ud_make_class(_x, countof_m1(_x))
#define UD_MAKE_NAME(_x)	ud_make_name(_x, countof_m1(_x))

/**
 * Print. */
extern uint8_t ud_fprint_tlv(const char *buf, void *fp);

/**
 * A ludicrously fast parser for keywords.
 * This does not use flex/bison but a trie based approach.
 * This can also parse stuff like :0x7e or :7e to allow for raw keys.
 *
 * At the moment the trie-based approach is supplanted by gperf. */
extern ud_tag_t ud_tag_from_s(const char *buf, size_t len);

/* tag-dependent dispatching of cli input */
extern unsigned int
ud_disp_tag(char *restrict buf, ud_tag_t t, const char *str, size_t len);


/* server specific stuff */
#if defined UNSERSRV
typedef long unsigned int hcode_t;

extern catng_t make_catalogue(void);
extern void free_catalogue(catng_t cat);

extern void catalogue_add_instr(catng_t cat, const instr_t);
#endif	/* UNSERSRV */

#endif	/* INCLUDED_catalogue_h_ */
