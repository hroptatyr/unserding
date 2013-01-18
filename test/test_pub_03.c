/*** test_pub_03.c -- testing publishing sockets */
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
		assert(s->fd > 0);

		res += ud_pack(s, "TEST", 4);
		res += ud_flush(s);
		res += ud_close(s);
	}
	return res;
}

/* test_pub_03.c ends here */
