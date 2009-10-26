#include <stdio.h>
#include <stdbool.h>
#define __USE_XOPEN
#include <time.h>
#include <pfack/uterus.h>
#include "unserding.h"
#include "protocore.h"
#include "tseries.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

static void
t1(spDute_t t)
{
	fprintf(stdout, "  ts:%i o:%2.4f h:%2.4f l:%2.4f c:%2.4f v:%2.4f\n",
		t->pivot,
		ffff_monetary32_d(t->cdl.o),
		ffff_monetary32_d(t->cdl.h),
		ffff_monetary32_d(t->cdl.l),
		ffff_monetary32_d(t->cdl.c),
		ffff_monetary64_d(t->cdl.v));
	return;
}

static void
ne(spDute_t t)
{
	fprintf(stdout, "  ts:%i v:does not exist\n", t->pivot);
	return;
}

static void
oh(spDute_t t)
{
	fprintf(stdout, "  ts:%i v:deferred\n", t->pivot);
	return;
}

static void
t_cb(spDute_t t, void *clo)
{
	fprintf(stdout, "tick storm, ticks:1 ii:%u/%u/%u tt:%i",
		t->instr, t->unit, (t->mux >> 6)/*getter?*/,
		(t->mux & 0x3f)/*getter?*/);

	switch (t->cdl.o) {
	case UTE_NEXIST:
		ne(t);
		break;
	case UTE_ONHOLD:
		oh(t);
		break;
	default:
		t1(t);
		break;
	}
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
	uint32_t bs = PFTB_BID | PFTB_ASK | PFTB_TRA | PFTB_STL | PFTB_FIX;

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
		if ((ts[n] = parse_time(argv[i])) == 0) {
			fprintf(stderr, "invalid date format \"%s\", "
				"must be YYYY-MM-DDThh:mm:ss\n", argv[i]);
			exit(1);
		}
		n++;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6);
	/* now kick off the finder */
	ud_find_ticks_by_instr(hdl, t_cb, NULL, &cid, bs, ts, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
