/*** test_pub_02.c -- testing publishing sockets */
#include <unserding.h>
#include <assert.h>

int
main(void)
{
	ud_sock_t s;
	int res = 0;

	if ((s = ud_socket((struct ud_sockopt_s){UD_PUB})) == NULL) {
		goto fuck;
	}

	assert(s->fd > 0);
	assert(s->fl == 0);
	assert(s->data == NULL);

	(void)ud_close(s);
fuck:
	return res;
}

/* test_pub_02.c ends here */
