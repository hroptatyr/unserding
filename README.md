unserding
=========

[![Build Status](https://secure.travis-ci.org/hroptatyr/unserding.png?branch=master)](http://travis-ci.org/hroptatyr/unserding)

unserding is a simple pub-sub messaging library, much like [0mq][1] or
[nanomsg][2], without all the transports they support and without the
reliability promise, made for heavy-duty realtime delivery of time
series.

unserding uses udp6 multicast to span ad-hoc topologies (on `ff0x::134`)
and uses a simple tag-length-value wire protocol to propagate messages.

+ github page: <https://github.com/hroptatyr/unserding>
+ downloads: <https://bitbucket.org/hroptatyr/unserding/downloads>


C API
-----
The C API is similar to [pimmel's][3] in design:

```c
/* for the waiter */
ud_sock_t s = ud_socket(UD_SUB);

while (pselect|poll|epoll(s->fd, ...)) {
        struct ud_msg_s msg[1];

        while (ud_chck_msg(s, msg) >= 0) {
                /* inspect the message contents */
                msg->data ...
        }
        break;
}
ud_close(s);
```

where `pselect`, `poll`, or `epoll` guts have been omitted for clarity.

The publisher part is similarly simple:

```c
/* for the notifier */
ud_sock_t s = ud_socket(UD_PUB);

/* pack a simple message */
ud_pack_msg(s, (ud_msg_s){.svc = 0xffff/*test service*/,
		.dlen = 4,
		.data = "TEST",
	});
ud_flush(s);
ud_close(s);
```

Examples
--------
Several dedicated projects utilise unserding networks to pass on
messages, typically in the field of financial trading systems.

Most notable among these is [unsermarkt][4], which in turn uses the tick
encoding capabilities of [uterus][5] to provide real-time tick data
streaming.  And moreover it defines a service (that can be plugged into
unsermon) to encode and decode such streams.

The [unsermarkt project][4] (aided by e.g. [twsgluum][6]) also
demonstrates how to send or receive FIX messages in unserding networks.

  [1]: https://github.com/zeromq/libzmq
  [2]: https://github.com/250bpm/nanomsg
  [3]: https://github.com/hroptatyr/pimmel
  [4]: https://github.com/hroptatyr/unsermarkt
  [5]: https://github.com/hroptatyr/uterus
  [6]: https://github.com/hroptatyr/twsgluum
