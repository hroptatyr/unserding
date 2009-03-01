/*** mod-instr-fx.c -- unserding module for fx instruments
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include <pfack/instruments.h>
#include "catalogue.h"

#if defined HAVE_FFFF_FFFF_H
/* to get a default set of aliases */
# define USE_MONETARY64
# include <ffff/ffff.h>
#else  /* !FFFF */
/* posix */
# include <math.h>
#endif	/* FFFF */
#include "catalogue-ng.h"

typedef long int timestamptz_t;
extern void mod_instr_fx_LTX_init(void);
extern void mod_instr_fx_LTX_deinit(void);

static inline void __attribute__((always_inline))
catalogue_add_ccy_instr(const_pfack_4217_t ccy, unsigned int cod)
{
	void *tmp = make_currency(ccy);
	catalogue_add_instr(instruments, tmp, cod);
	return;
}

static void
catalogue_add_fx_instr(
	const_pfack_4217_t base, const_pfack_4217_t terms, unsigned int cod)
{
	char newn[3 + 3 + 1];
	void *tmp;

	/* create the name of the pair */
	newn[0] = base->sym[0],
		newn[1] = base->sym[1],
		newn[2] = base->sym[2];
	newn[3] = terms->sym[0],
		newn[4] = terms->sym[1],
		newn[5] = terms->sym[2];
	newn[6] = '\0';

	/* create the instrument */
	tmp = make_fxpair(newn, NULL, NULL);
	catalogue_add_instr(instruments, tmp, cod);
	return;
}

static void
obtain_some_4217s(void)
{
	/* currencies */
	catalogue_add_ccy_instr(PFACK_4217_EUR, 0x80000001);
	catalogue_add_ccy_instr(PFACK_4217_USD, 0x80000002);
	catalogue_add_ccy_instr(PFACK_4217_GBP, 0x80000003);
	catalogue_add_ccy_instr(PFACK_4217_CAD, 0x80000004);
	catalogue_add_ccy_instr(PFACK_4217_AUD, 0x80000005);
	catalogue_add_ccy_instr(PFACK_4217_KRW, 0x80000006);
	catalogue_add_ccy_instr(PFACK_4217_JPY, 0x80000007);
	catalogue_add_ccy_instr(PFACK_4217_INR, 0x80000008);
	catalogue_add_ccy_instr(PFACK_4217_HKD, 0x80000009);
	catalogue_add_ccy_instr(PFACK_4217_CHF, 0x8000000a);
	catalogue_add_ccy_instr(PFACK_4217_CNY, 0x8000000b);
	catalogue_add_ccy_instr(PFACK_4217_RUB, 0x8000000c);
	catalogue_add_ccy_instr(PFACK_4217_BRL, 0x8000000d);
	catalogue_add_ccy_instr(PFACK_4217_MXN, 0x8000000e);
	catalogue_add_ccy_instr(PFACK_4217_SEK, 0x8000000f);
	catalogue_add_ccy_instr(PFACK_4217_NOK, 0x80000010);
	catalogue_add_ccy_instr(PFACK_4217_NZD, 0x80000011);
	catalogue_add_ccy_instr(PFACK_4217_CLP, 0x80000012);

	/* precious metals, they should be TIXXXX */
	catalogue_add_ccy_instr(PFACK_4217_XAU, 0x80000100);
	catalogue_add_ccy_instr(PFACK_4217_XAG, 0x80000101);
	catalogue_add_ccy_instr(PFACK_4217_XPT, 0x80000102);

	/* some usual pairs */
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_USD, 0xc0000001);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_GBP, 0xc0000002);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_CAD, 0xc0000003);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_AUD, 0xc0000004);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_KRW, 0xc0000005);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_JPY, 0xc0000006);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_INR, 0xc0000007);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_HKD, 0xc0000008);
	catalogue_add_fx_instr(PFACK_4217_EUR, PFACK_4217_CHF, 0xc0000009);
	return;
}

void
mod_instr_fx_LTX_init(void)
{
	obtain_some_4217s();
	return;
}

void
mod_instr_fx_LTX_deinit(void)
{
	return;
}

/* mod-instr-fx.c ends here */
