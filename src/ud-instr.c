#include <stdio.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "protocore.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

static void
fjfj(char *buf, size_t len, void *clo)
{
	instr_t in = deser_instrument(buf, len);
	fprintf(stderr, "d/l'd instrument: ");
	print_instr(stderr, in);
	fputc('\n', stderr);
	return;
}

int
main(int argc, const char *argv[])
{
	char buf[UDPC_PLLEN];
	size_t len;
	uint32_t cid[64];
	int n = 0;

	for (int i = 1; i < argc; i++) {
		char *p;
		cid[n++] = strtol(argv[i], &p, 10);
	}
	if (n == 0) {
		return 0;
	}

	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6);
	/* now kick off the finder */
	ud_find_many_instrs(hdl, fjfj, NULL, cid, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-instr.c ends here */
