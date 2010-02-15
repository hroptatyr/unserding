/* to be included somewhere */

/* include this directly? */
#include <sushi/m30.h>

/**
 * Given a scom object T, return its tick type as a character,
 * for the purpose of printing it. */
static char
ttc(scom_t t)
{
	switch (scom_thdr_ttf(t)) {
	case SCOM_FLAG_LM | SL1T_TTF_BID:
	case SL1T_TTF_BID:
		return 'b';
	case SCOM_FLAG_LM | SL1T_TTF_ASK:
	case SL1T_TTF_ASK:
		return 'a';
	case SCOM_FLAG_LM | SL1T_TTF_TRA:
	case SL1T_TTF_TRA:
		return 't';
	case SCOM_FLAG_LM | SL1T_TTF_STL:
	case SL1T_TTF_STL:
		return 'x';
	case SCOM_FLAG_LM | SL1T_TTF_FIX:
	case SL1T_TTF_FIX:
		return 'f';
	default:
		return 'u';
	}
}

/**
 * Print an ordinary sparse level 1 tick. */
static void
t1(scom_t t)
{
	const_sl1t_t tv = (const void*)t;
	double v0 = ffff_m30_d(ffff_m30_get_ui32(tv->v[0]));
	double v1 = ffff_m30_d(ffff_m30_get_ui32(tv->v[1]));

	fputc(' ', stdout);
	fputc(' ', stdout);
	fputc(ttc(t), stdout);

	fprintf(stdout, ":%2.4f %2.4f\n", v0, v1);
	return;
}

/**
 * Print a candle made of sl1 ticks. */
static void
t1c(scom_t t)
{
	const_scdl_t tv = (const void*)t;
	double o = ffff_m30_d(ffff_m30_get_ui32(tv->o));
	double h = ffff_m30_d(ffff_m30_get_ui32(tv->h));
	double l = ffff_m30_d(ffff_m30_get_ui32(tv->l));
	double c = ffff_m30_d(ffff_m30_get_ui32(tv->c));
	int32_t v = tv->cnt;
	int32_t bts = tv->sta_ts;

	fprintf(stdout, " o:%2.4f h:%2.4f l:%2.4f c:%2.4f v:%i  b:%i\n",
		o, h, l, c, v, bts);
	return;
}

/**
 * Print an sl1 market snap shot. */
static void
t1s(scom_t t)
{
	const_ssnap_t tv = (const void*)t;
	double bp = ffff_m30_d(ffff_m30_get_ui32(tv->bp));
	double ap = ffff_m30_d(ffff_m30_get_ui32(tv->ap));
	double bq = ffff_m30_d(ffff_m30_get_ui32(tv->bq));
	double aq = ffff_m30_d(ffff_m30_get_ui32(tv->aq));
	double tvpr = ffff_m30_d(ffff_m30_get_ui32(tv->tvpr));
	double tq = ffff_m30_d(ffff_m30_get_ui32(tv->tq));

	fprintf(stdout,
		" b:%2.4f bs:%2.4f  a:%2.4f as:%2.4f "
		" tvpr:%2.4f tq:%2.4f\n",
		bp, bq, ap, aq, tvpr, tq);
	return;
}

/**
 * For ticks which are guaranteed not to exist, (easter, christmas, etc.) */
static void
ne(scom_t UNUSED(t))
{
	fputs("  v:does not exist\n", stdout);
	return;
}

/**
 * For ticks which are on-hold, which means that the data source has
 * noticed the request but cannot currently deliver the data. */
static void
oh(scom_t UNUSED(t))
{
	fputs("  v:deferred\n", stdout);
	return;
}

/**
 * Pretty-print the secu S. */
static __attribute__((unused)) void
pretty_print_secu(su_secu_t s)
{
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);

	fprintf(stdout, "tick storm: ii:%u/%i@%hu", qd, qt, p);
	return;
}

/**
 * Pretty-print the tick T. */
static __attribute__((unused)) void
pretty_print_scom(scom_t t)
{
	uint16_t ttf = scom_thdr_ttf(t);
	char ttfc = ttc(t);
	int32_t ts = scom_thdr_sec(t);
	uint16_t ms = scom_thdr_msec(t);
	char tss[32];

	print_ts_into(tss, sizeof(tss), ts);
	fprintf(stdout, "  tt:%c  ts:%s.%03hu", ttfc, tss, ms);

	if (scom_thdr_nexist_p(t)) {
		ne(t);
	} else if (scom_thdr_onhold_p(t)) {
		oh(t);
	} else if (!scom_thdr_linked(t)) {
		t1(t);
	} else if (ttf == SSNP_FLAVOUR) {
		t1s(t);
	} else if (ttf > SCDL_FLAVOUR) {
		t1c(t);
	}
	return;
}

/* utehelper.c ends here */
