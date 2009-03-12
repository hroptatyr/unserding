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

typedef long int timestamptz_t;
extern void mod_instr_fx_LTX_init(void);
extern void mod_instr_fx_LTX_deinit(void);

#define UD_IDXOF(_x)	(PFACK_4217_##_x##_IDX)
#define UD_CCY_ID(_x)	(UD_IDXOF(_x) | 0x80000000)
#define UD_4217_ID(_x)	((_x) & ~0x80000000)

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
catalogue_add_tr_instr(instr_id_t id, const char *name)
{
	instr_t tmp = make_txxxxx(id, name, "TRXXXX");
	catalogue_add_instr(instruments, tmp);
	return;
}

static void
obtain_some_4217s(void)
{
	/* currencies */
	catalogue_add_ccy_instr(PFACK_4217_EUR, UD_CCY_ID(EUR));
	catalogue_add_ccy_instr(PFACK_4217_USD, UD_CCY_ID(USD));
	catalogue_add_ccy_instr(PFACK_4217_GBP, UD_CCY_ID(GBP));
	catalogue_add_ccy_instr(PFACK_4217_CAD, UD_CCY_ID(CAD));
	catalogue_add_ccy_instr(PFACK_4217_AUD, UD_CCY_ID(AUD));
	catalogue_add_ccy_instr(PFACK_4217_KRW, UD_CCY_ID(KRW));
	catalogue_add_ccy_instr(PFACK_4217_JPY, UD_CCY_ID(JPY));
	catalogue_add_ccy_instr(PFACK_4217_INR, UD_CCY_ID(INR));
	catalogue_add_ccy_instr(PFACK_4217_HKD, UD_CCY_ID(HKD));
	catalogue_add_ccy_instr(PFACK_4217_CHF, UD_CCY_ID(CHF));
	catalogue_add_ccy_instr(PFACK_4217_CNY, UD_CCY_ID(CNY));
	catalogue_add_ccy_instr(PFACK_4217_RUB, UD_CCY_ID(RUB));
	catalogue_add_ccy_instr(PFACK_4217_BRL, UD_CCY_ID(BRL));
	catalogue_add_ccy_instr(PFACK_4217_MXN, UD_CCY_ID(MXN));
	catalogue_add_ccy_instr(PFACK_4217_SEK, UD_CCY_ID(SEK));
	catalogue_add_ccy_instr(PFACK_4217_NOK, UD_CCY_ID(NOK));
	catalogue_add_ccy_instr(PFACK_4217_NZD, UD_CCY_ID(NZD));
	catalogue_add_ccy_instr(PFACK_4217_CLP, UD_CCY_ID(CLP));

	/* precious metals, they should be TIXXXX */
	catalogue_add_ccy_instr(PFACK_4217_XAU, UD_CCY_ID(XAU));
	catalogue_add_ccy_instr(PFACK_4217_XAG, UD_CCY_ID(XAG));
	catalogue_add_ccy_instr(PFACK_4217_XPT, UD_CCY_ID(XPT));

	/* some usual pairs */
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(USD), 0xc0000001);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(GBP), 0xc0000002);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(CAD), 0xc0000003);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(AUD), 0xc0000004);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(KRW), 0xc0000005);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(JPY), 0xc0000006);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(INR), 0xc0000007);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(HKD), 0xc0000008);
	catalogue_add_fx_instr(UD_IDXOF(EUR), UD_IDXOF(CHF), 0xc0000009);

	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(CAD), 0xc0000010);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(AUD), 0xc0000011);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(KRW), 0xc0000012);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(JPY), 0xc0000013);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(INR), 0xc0000014);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(HKD), 0xc0000015);
	catalogue_add_fx_instr(UD_IDXOF(USD), UD_IDXOF(CHF), 0xc0000016);

	catalogue_add_fx_instr(UD_IDXOF(GBP), UD_IDXOF(USD), 0xc0000020);
	catalogue_add_fx_instr(UD_IDXOF(GBP), UD_IDXOF(CAD), 0xc0000021);
	catalogue_add_fx_instr(UD_IDXOF(GBP), UD_IDXOF(AUD), 0xc0000022);

	catalogue_add_fx_instr(UD_IDXOF(XAU), UD_IDXOF(USD), 0xc0000100);
	catalogue_add_fx_instr(UD_IDXOF(XAG), UD_IDXOF(USD), 0xc0000101);
	catalogue_add_fx_instr(UD_IDXOF(XPT), UD_IDXOF(USD), 0xc0000102);
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

static void
obtain_interests(void)
{
	catalogue_add_tr_instr(0xe0000001, "FFD");
	/* EONIA Eonia is computed as a weighted average of all overnight
	   unsecured lending transactions in the interbank market, initiated
	   within the euro area by the Panel Banks. It is reported on an act/360
	   day count convention and is displayed to two decimal places. Eonia
	   is to move to 3 decimals on 3rd September 2007 (view Press Release).

	   'Overnight' means from one TARGET day (i.e. day on which the
	   Trans-European Automated Real-Time Gross-Settlement Express Transfer
	   system is open) to the next TARGET day.

	   The panel of reporting banks is the same as for Euribor, so that
	   only the most active banks located in the euro area are represented
	   on the panel and the geographical diversity of banks in the panel is
	   maintained. */
	catalogue_add_tr_instr(0xe0000002, "EONIA");

	/* EURIBOR A representative panel of banks provide daily quotes of the
	   rate, rounded to two decimal places, that each panel bank believes
	   one prime bank is quoting to another prime bank for interbank term
	   deposits within the euro zone.

	   Euribor is quoted for spot value (T+2) and on an act/360 day-count
	   convention. It is displayed to three decimal places.

	   Panel Banks contribute for one, two and three weeks and for twelve
	   maturities from one to twelve months. */
	catalogue_add_tr_instr(0xe0000003, "Euribor");
	return;
}

static void
obtain_options(void)
{
	/* just the S&P P3800 example */
	instr_t tmp = make_oxxxxx(
		(instr_id_t)42083U, "S&P C1900 2008-06", "OPEICS", "XCBO");
	/* set funding */
	instr_funding_set_fund_instr(tmp, UD_CCY_ID(USD));
	instr_funding_set_setd_instr(tmp, UD_CCY_ID(USD));
	/* delivery group */
	instr_delivery_set_issue(tmp, 0);
	instr_delivery_set_expiry(tmp, 14049);
	instr_delivery_set_settle(tmp, 14052);
	/* referential group */
	instr_referent_set_underlyer(tmp, (instr_id_t)15);
	instr_referent_set_strike(tmp, ffff_monetary_get_d(1900.0));
	instr_referent_set_ratio2(tmp, 1, 1);
	/* now add him */
	catalogue_add_instr(instruments, tmp);
	return;
}

void
mod_instr_fx_LTX_init(void)
{
	obtain_some_4217s();
	obtain_indices();
	obtain_interests();
	obtain_options();
	return;
}

void
mod_instr_fx_LTX_deinit(void)
{
	return;
}

/* mod-instr-fx.c ends here */
