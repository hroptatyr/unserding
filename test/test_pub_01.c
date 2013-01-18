/*** test_pub_01.c -- testing publishing sockets */
#include <unserding.h>

int
main(void)
{
	ud_sock_t s;
	int res = 0;

	if ((s = ud_socket((struct ud_sockopt_s){UD_PUB})) == NULL) {
		res = 1;
	} else if (ud_close(s) < 0) {
		res = 1;
	}
	return res;
}

/* test_pub_01.c ends here */
