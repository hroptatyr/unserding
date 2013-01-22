/*** test_pub_04.c -- testing publishing sockets */
#include <unserding.h>
#include <assert.h>

int
main(void)
{
	ud_sock_t s;
	int res = 0;

	if ((s = ud_socket((struct ud_sockopt_s){
				   UD_PUB, .addr = UD_MCAST6_NODE_LOCAL,
					   .port = 8378/*TEST*/}))) {
		ud_svc_t svc;

		assert(s->fd > 0);

		/* testing the reader  */
		if (ud_chck(&svc, NULL, 0, s) > 0) {
			res = 1;
		}
		res += ud_close(s);
	}
	return res;
}

/* test_pub_04.c ends here */
