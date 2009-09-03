#include <stdio.h>
#include <stdbool.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "protocore.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;
static bool xmlp;

static void
fjfj(char *buf, size_t len, void *clo)
{
	struct instr_s in;

	deser_instrument_into(&in, buf, len);
	fprintf(stderr, "d/l'd instrument: ");
	print_instr(stderr, &in);
	fputc('\n', stderr);
	return;
}

int
main(int argc, const char *argv[])
{
	/* vla */
	uint32_t cid[argc];
	int n = 0;

	for (int i = 1; i < argc; i++) {
		if ((cid[n] = strtol(argv[i], NULL, 10))) {
			n++;
		} else if (strcmp(argv[i], "--xml") == 0) {
			xmlp = true;
		}
	}
	if (n == 0) {
		return 0;
	}
	printf("sizeof %d\n", sizeof(struct instr_s));
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6);
	/* now kick off the finder */
	ud_find_many_instrs(hdl, fjfj, NULL, cid, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-instr.c ends here */
