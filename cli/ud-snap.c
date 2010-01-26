#include <stdio.h>
#include <stdbool.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"
#include <sushi/m30.h>

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;
static bool UNUSED(xmlp);

static void
t_cb(su_secu_t s, scom_t th, void *UNUSED(clo))
{
	const_sl1t_t t = (const void*)th;
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
	uint16_t ttf = scom_thdr_ttf(th);
	time_t ts = scom_thdr_sec(th);
	uint16_t ms = scom_thdr_msec(th);
	double v = ffff_m30_d(ffff_m30_get_ui32(t->v[0]));

	fprintf(stdout, "ii:%u/%i@%hu  tt:%d ts:%ld.%03hd v:%2.4f\n",
		qd, qt, p, ttf, ts, ms, v);
	return;
}

int
main(int argc, const char *argv[])
{
	/* vla */
	su_secu_t cid[argc];
	int n = 0;
	time_t ts = time(NULL);
	uint32_t bs = ~0;

	for (int i = 1; i < argc; i++) {
		uint32_t qd = strtol(argv[i], NULL, 10);
		if (qd) {
			cid[n] = su_secu(qd, 0, 0);
			n++;
		} else if (strcmp(argv[i], "--xml") == 0) {
			xmlp = true;
		}
	}
	if (n == 0) {
		return 0;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* now kick off the finder */
	ud_find_ticks_by_ts(hdl, t_cb, NULL, cid, n, bs, ts);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-snap.c ends here */
