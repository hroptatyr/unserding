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
	instr_t tmp = make_txxxxx(cod, ccy->sym, "TCXXXX");
	catalogue_add_instr(instruments, tmp);
	return;
}

static void
catalogue_add_fx_instr(
	pfack_4217_id_t base, pfack_4217_id_t terms, unsigned int cod)
{
	instr_t tmp = make_ffcpnx(cod, base, terms);
	catalogue_add_instr(instruments, tmp);
	return;
}

static void
catalogue_add_ti_instr(instr_id_t id, const char *name)
{
	instr_t tmp = make_txxxxx(id, name, "TIXXXX");
	catalogue_add_instr(instruments, tmp);
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
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_USD_IDX, 0xc0000001);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_GBP_IDX, 0xc0000002);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_CAD_IDX, 0xc0000003);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_AUD_IDX, 0xc0000004);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_KRW_IDX, 0xc0000005);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_JPY_IDX, 0xc0000006);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_INR_IDX, 0xc0000007);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_HKD_IDX, 0xc0000008);
	catalogue_add_fx_instr(
		PFACK_4217_EUR_IDX, PFACK_4217_CHF_IDX, 0xc0000009);

	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_CAD_IDX, 0xc0000010);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_AUD_IDX, 0xc0000011);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_KRW_IDX, 0xc0000012);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_JPY_IDX, 0xc0000013);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_INR_IDX, 0xc0000014);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_HKD_IDX, 0xc0000015);
	catalogue_add_fx_instr(
		PFACK_4217_USD_IDX, PFACK_4217_CHF_IDX, 0xc0000016);

	catalogue_add_fx_instr(
		PFACK_4217_GBP_IDX, PFACK_4217_USD_IDX, 0xc0000020);
	catalogue_add_fx_instr(
		PFACK_4217_GBP_IDX, PFACK_4217_CAD_IDX, 0xc0000021);
	catalogue_add_fx_instr(
		PFACK_4217_GBP_IDX, PFACK_4217_AUD_IDX, 0xc0000022);

	catalogue_add_fx_instr(
		PFACK_4217_XAU_IDX, PFACK_4217_USD_IDX, 0xc0000100);
	catalogue_add_fx_instr(
		PFACK_4217_XAG_IDX, PFACK_4217_USD_IDX, 0xc0000101);
	catalogue_add_fx_instr(
		PFACK_4217_XPT_IDX, PFACK_4217_USD_IDX, 0xc0000102);
	return;
}

static void
obtain_indices(void)
{
	catalogue_add_ti_instr(1, "DAX");
	catalogue_add_ti_instr(3, "ESX");
	catalogue_add_ti_instr(5, "CAC");
	catalogue_add_ti_instr(7, "FTSE");
	catalogue_add_ti_instr(9, "DJX");
	catalogue_add_ti_instr(11, "NDX");
	catalogue_add_ti_instr(13, "RUT");
	catalogue_add_ti_instr(15, "SPX");
	catalogue_add_ti_instr(17, "XEO");
	catalogue_add_ti_instr(19, "K200");
	return;
}

void
mod_instr_fx_LTX_init(void)
{
	obtain_some_4217s();
	obtain_indices();
	return;
}

void
mod_instr_fx_LTX_deinit(void)
{
	return;
}

/* mod-instr-fx.c ends here */
