#include <sushi/secu.h>

#define SLASH_FOUND	1
#define ATSYM_FOUND	2

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
	qd = strtoul(s, NULL, 10);
	if (slp) {
		qt = strtol(slp, NULL, 10);
	}
	if (atp) {
		p = strtoul(atp, NULL, 10);
	}
	printf("parsed secu %u/%i@%hu\n", qd, qt, p);
	return res;
}

