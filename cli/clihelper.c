/* to be included somewhere */
#include <sushi/secu.h>
#include <pfack/instruments.h>

#define SLASH_FOUND	1
#define ATSYM_FOUND	2

static int32_t
gaid_from_str(ud_handle_t hdl, const char *s, size_t len)
{
	char *ends;
	long int i = strtol(s, &ends, 10);

	if (ends == s) {
		/* probably a string, we look her up now */
		struct instr_s in[1];
		const void *data;
		size_t ninstr;
		if ((ninstr = ud_find_one_isym(hdl, &data, s, len)) > 0) {
			deser_instrument_into(in, data, ninstr);
			return instr_gaid(in);
		}
	}
	return (int32_t)i;
}

static uint16_t
potid_from_str(ud_handle_t UNUSED(hdl), const char *s, size_t UNUSED(len))
{
	char *ends;
	long int i = strtol(s, &ends, 10);

	if (ends == s) {
		return 0;
	}
	return (uint16_t)i;
}

static su_secu_t
secu_from_str(ud_handle_t hdl, const char *s)
{
/* we support "a" and "a/b@c" syntax, use `0' or `*' as placeholder,
 * "a" is expanded to "a/0@0"
 * we need the handle for lookups */
	su_secu_t res;
	const char *ends, *slp, *atp;
	uint32_t qd = 0;
	int32_t qt = 0;
	uint16_t p = 0;

	/* check if we have a/b@c syntax */
	for (ends = s, slp = atp = NULL; *ends != '\0'; ends++) {
		if (*ends == '/') {
			slp = ends + 1;
		} else if (*ends == '@') {
			atp = ends + 1;
		}
	}
	qd = gaid_from_str(
		hdl, s, slp ? slp - s - 1 : (atp ? atp - s - 1 : ends - s));
	if (slp) {
		qt = gaid_from_str(hdl, slp, atp ? atp - slp - 1 : ends - slp);
	}
	if (atp) {
		p = potid_from_str(hdl, atp, ends - atp);
	}
#if defined DEBUG_FLAG
	fprintf(stderr, "parsed secu %u/%i@%hu\n", qd, qt, p);
#endif	/* DEBUG_FLAG */
	return res;
}

