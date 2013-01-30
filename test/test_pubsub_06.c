/*** test_pubsub_06.c -- testing publishing sockets */
#include <unserding.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <poll.h>

#define countof(x)		(sizeof(x) / sizeof(*(x)))

static const char secret[] = "JUST A PLAIN STRING";

static int
poll_send(ud_sock_t s)
{
	struct pollfd fds[1];
	int rc;
	int timeout = 2000;

	fds->fd = s->fd;
	fds->events = POLLOUT;

	if ((rc = poll(fds, countof(fds), timeout)) <= 0) {
		perror("socket not ready for sending");
		return -1;
	} else if (!(fds->revents & POLLOUT)) {
		perror("socket not ready for sending, despite poll");
		return -1;
	} else if (ud_pack_msg(s, (struct ud_msg_s){
				.svc = 0xffff/*TEST SERVICE*/,
				.data = secret,
				.dlen = sizeof(secret),
			}) < 0) {
		perror("couldn't pack secret message");
		return -1;
	}
	return ud_flush(s);
}

static int
poll_recv(ud_sock_t s)
{
	struct ud_msg_s msg[1];
	struct pollfd fds[1];
	int rc;
	int timeout = 2000;

	fds->fd = s->fd;
	fds->events = POLLIN;

	if ((rc = poll(fds, countof(fds), timeout)) < 0) {
		perror("socket not ready for recving");
		return -1;
	} else if (rc == 0) {
		perror("socket timed out");
	} else if (!(fds->revents & POLLIN)) {
		perror("socket not ready for recving, despite poll");
		return -1;
	} else if (ud_chck_msg(msg, s) < 0) {
		perror("message received but b0rked");
		return -1;
	} else if (msg->svc != 0xffff) {
		perror("not the test message we sent");
		return -1;
	} else if (msg->dlen != sizeof(secret)) {
		perror("data lengths do not coincide");
		return -1;
	} else if (memcmp(msg->data, secret, sizeof(secret))) {
		perror("data contents do not coincide");
		return -1;
	}
	return 0;
}

int
main(void)
{
	ud_sock_t s;
	int res = 0;

	if ((s = ud_socket((struct ud_sockopt_s){UD_PUBSUB})) == NULL) {
		perror("cannot initialise ud socket");
		return 1;
	}

	assert(s->fd > 0);

	if (poll_send(s) < 0) {
		res = 1;
		goto fuck;
	}

	if (poll_recv(s) < 0) {
		res = 1;
		goto fuck;
	}

fuck:
	res -= ud_close(s);
	return res;
}

/* test_pubsub_06.c ends here */
