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
	const char *ends;
	int state = 0;

	/* check if we have a/b@c syntax */
	for (ends = s; *s != '\0'; s++) {
		if (*s == '/') {
			state |= SLASH_FOUND;
		} else if (*s == '@') {
			state |= ATSYM_FOUND;
		}
	}
	printf("state %x\n", state);
	return res;
}

