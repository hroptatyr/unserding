#include <stdio.h>
#include <stdbool.h>
#define __USE_XOPEN
#include <time.h>
#include <pfack/tick.h>
#include "unserding.h"
#include "protocore.h"
#include "xdr-instr-seria.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

static void
t_cb(sl1tick_t t, void *clo)
{
	fprintf(stdout, "ii:%u  tt:%d ts:%ld.%03d v:%2.4f\n",
		sl1tick_instr(t),
		sl1tick_tick_type(t),
		(long int)sl1tick_timestamp(t),
		(short int)sl1tick_msec(t),
		ffff_monetary32_d(sl1tick_value(t)));
	return;
}

static time_t
parse_time(const char *t)
{
	struct tm tm;
	char *on;

	memset(&tm, 0, sizeof(tm));
	on = strptime(t, "%Y-%m-%d", &tm);
	if (on == NULL) {
		return 0;
	}
	if (on[0] == ' ' || on[0] == 'T' || on[0] == '\t') {
		on++;
	}
	(void)strptime(on, "%H:%M:%S", &tm);
	return timegm(&tm);
}

int
main(int argc, const char *argv[])
{
	/* vla */
	struct secu_s cid;
	int n = 0;
	time_t ts[argc-1];
	uint32_t bs = PFTB_EOD | PFTB_STL;

	if (argc <= 1) {
		fprintf(stderr, "Usage: ud-tick instr [date] [date] ...\n");
		exit(1);
	}
	/* we've got at least the instr id */
	cid.instr = strtol(argv[1], NULL, 10);
	cid.unit = 0;
	cid.pot = 0;

	if (argc == 2) {
		ts[0] = time(NULL);
	}

	for (int i = 2; i < argc; i++) {
		if ((ts[0] = parse_time(argv[i])) == 0) {
			fprintf(stderr, "invalid date format \"%s\", "
				"must be YYYY-MM-DDThh:mm:ss\n", argv[i]);
			exit(1);
		}
		n++;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6);
	/* now kick off the finder */
	ud_find_ticks_by_ts(hdl, t_cb, NULL, &cid, 1, bs, ts[0]);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
