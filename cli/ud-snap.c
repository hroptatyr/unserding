#include <stdio.h>
#include <stdbool.h>
#include <pfack/uterus.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;
static bool xmlp;

static void
t_cb(sl1tick_t t, void *UNUSED(clo))
{
	uint32_t i = sl1tick_instr(t);
	int tt = (int)sl1tick_tick_type(t);
	long int s = (long int)sl1tick_timestamp(t);
	short int ms = (short int)sl1tick_msec(t);
	double v = ffff_monetary32_d(sl1tick_value(t));

	fprintf(stdout, "ii:%u  tt:%d ts:%ld.%03hd v:%2.4f\n", i, tt, s, ms, v);
	return;
}

int
main(int argc, const char *argv[])
{
	/* vla */
	struct secu_s cid[argc];
	int n = 0;
	time_t ts = time(NULL);
	uint32_t bs = PFTB_EOD | PFTB_STL;

	for (int i = 1; i < argc; i++) {
		if ((cid[n].instr = strtol(argv[i], NULL, 10))) {
			cid[n].unit = 0;
			cid[n].pot = 0;
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
