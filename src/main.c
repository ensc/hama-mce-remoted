/*	--*- c -*--
 * Copyright (C) 2010 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <linux/input.h>

static const enum { OUT_LIRC, OUT_DEV }		OUT_MODE = OUT_LIRC;

enum {
	MODE_META	=  (1 << 0),
	MODE_CTRL	=  (1 << 1),
	MODE_SHIFT	=  (1 << 2),
	MODE_ALT	=  (1 << 3),
	MODE_NUMLOCK	=  (1 << 4),
	MODE_NUMALT	=  (1 << 5),
};

struct key_definition {
	unsigned long	mask;
	unsigned int	key;

	unsigned short	code;
	char const	*str;
};

struct input_state {
	int			fd;
	unsigned long		mode;
	signed int		key;
	bool			is_alt;

	struct input_event	ev;
};

#define M(_meta,_ctrl,_shift,_alt,_num) \
	(((_meta) ? MODE_META : 0) |	\
	 ((_ctrl) ? MODE_CTRL : 0) |	\
	 ((_shift) ? MODE_SHIFT : 0) |  \
	 ((_alt) ? MODE_ALT : 0) |	\
	 ((_num) ? MODE_NUMALT : 0))

#define D(_mask, _key, _code) \
	{ _mask, _key, _code, # _code }

static struct key_definition	KEY_DEFS[] = {
	D( 0, KEY_HOMEPAGE,          KEY_HOMEPAGE ),
	D( 0, KEY_SLEEP,             KEY_SLEEP ),

	D( M(0,1,1,0,0), 20,         KEY_YELLOW ),
	D( M(0,1,0,0,0), 50,         KEY_BLUE ),
	D( M(0,1,0,0,0), 23,         KEY_GREEN ),
	D( M(0,1,0,0,0), 18,         KEY_RED ),

	D( M(0,1,0,0,0), 24,         KEY_HOMEPAGE ),
	D( M(0,1,0,0,0), 34,         KEY_PROGRAM ),
	D( M(0,1,0,0,0), 20,         KEY_SCREEN ),
	D( M(0,1,1,0,0), 50,         KEY_DVD ),

	D( M(0,1,1,0,0), 48,         KEY_REWIND ),
	D( M(0,1,1,0,0), 33,	    KEY_FASTFORWARD ),
	D( 0, KEY_PREVIOUSSONG,      KEY_PREVIOUSSONG ),
	D( 0, KEY_NEXTSONG,          KEY_NEXTSONG ),

	D( 0, KEY_PLAYPAUSE,         KEY_PLAYPAUSE ),

	D( 0, KEY_STOPCD,            KEY_STOPCD ),
	D( M(0,1,0,0,0), 19,         KEY_RECORD ),

	D( 0, KEY_BACKSPACE,         KEY_BACKSPACE ),
	D( 0, BTN_RIGHT,		    KEY_INFO ),

	D( 0, KEY_LEFT,              KEY_LEFT ),
	D( 0, KEY_RIGHT,             KEY_RIGHT ),
	D( 0, KEY_UP,                KEY_UP ),
	D( 0, KEY_DOWN,              KEY_DOWN ),

	D( 0, KEY_ENTER,             KEY_ENTER ),
	D( 0, BTN_LEFT,              BTN_LEFT ),
/*	D( 0, BTN_RIGHT,             BTN_RIGHT ), */

	D( 0, KEY_VOLUMEUP,          KEY_VOLUMEUP ),
	D( 0, KEY_VOLUMEDOWN,        KEY_VOLUMEDOWN ),
	D( 0, KEY_MUTE,              KEY_MUTE ),

	D( 0, KEY_PAGEUP,            KEY_CHANNELUP ),
	D( 0, KEY_PAGEDOWN,          KEY_CHANNELDOWN ),

	D( M(1,0,0,1,0), 28,         BTN_START ),
	D( 0, KEY_KP1,               KEY_KP1 ),
	D( 0, KEY_KP2,               KEY_KP2 ),
	D( 0, KEY_KP3,               KEY_KP3 ),
	D( 0, KEY_KP4,               KEY_KP4 ),
	D( 0, KEY_KP5,               KEY_KP5 ),
	D( 0, KEY_KP6,               KEY_KP6 ),
	D( 0, KEY_KP7,               KEY_KP7 ),
	D( 0, KEY_KP8,               KEY_KP8 ),
	D( 0, KEY_KP9,               KEY_KP9 ),
	D( 0, KEY_KP0,               KEY_KP0 ),

	D( 0, KEY_KPASTERISK,        KEY_KPASTERISK ),
	D( M(0,0,0,0,1), '#',        KEY_KPPLUSMINUS ),
	D( M(0,0,0,1,0), 62,         KEY_CLOSE ),
	D( 0, KEY_ESC,               KEY_ESC ),
};

#undef D
#undef M

static void fill_keyname(char *dst, struct input_event const *ev, unsigned int code)
{
	size_t		i;
	char const	*prefix;

	switch (ev->type) {
	case EV_KEY:
		for (i = 0; i < sizeof KEY_DEFS / sizeof KEY_DEFS[0]; ++i) {
			if (KEY_DEFS[i].code == ev->code) {
				strcpy(dst, KEY_DEFS[i].str);
				return;
			}
		}

		prefix = "KEY_";
		break;
	case EV_REL:
		prefix = "REL_";
		break;
	default:
		prefix = "???_";
		break;
	}

	sprintf(dst, "%s%08x", prefix, code);
}

static bool fill_key(struct input_event *ev, unsigned long mask, unsigned int key)
{
	size_t		i;

	for (i = 0; i < sizeof KEY_DEFS / sizeof KEY_DEFS[0]; ++i) {
		if (KEY_DEFS[i].mask == mask &&
		    KEY_DEFS[i].key  == key) {
			ev->type = EV_KEY;
			ev->code = KEY_DEFS[i].code;

			return true;
		}
	}

	return false;
}

static void handle_input(struct input_state *st, int out_fd)
{
	struct input_event	ev;
	ssize_t			l;
	unsigned long		mask = 0, value;
	bool			do_submit = false;

	l = read(st->fd, &ev, sizeof ev);
	if (l == 0)
		exit(2);
	if (l < 0) {
		perror("read()");
		exit(1);
	}

	if (l != sizeof ev) {
		abort();
		/* TODO */
		return;
	}

	switch (ev.type) {
	case EV_KEY:
		switch (ev.code) {
		case KEY_LEFTMETA:
		case KEY_RIGHTMETA:
			mask = MODE_META;
			break;

		case KEY_LEFTCTRL:
		case KEY_RIGHTCTRL:
			mask = MODE_CTRL;
			break;

		case KEY_LEFTALT:
		case KEY_RIGHTALT:
			mask = MODE_ALT;
			break;

		case KEY_LEFTSHIFT:
		case KEY_RIGHTSHIFT:
			mask = MODE_SHIFT;
			break;

		case KEY_NUMLOCK:
			mask = MODE_NUMLOCK;
			break;
		}

		if (mask) {
			if (ev.value)
				value = mask;
			else
				value = 0;

			st->mode &= ~mask;
			st->mode |= value;

			if (st->mode != MODE_ALT && st->is_alt) {
				ev.value   = 1;
				do_submit  = true;
			}
		} else if (st->mode == MODE_ALT && ev.value == 0)
			;		/* noop */
		else if (st->mode == MODE_ALT) {
			int		num_code = -1;

			switch (ev.code) {
			case KEY_KP0:	num_code = 0; break;
			case KEY_KP1:	num_code = 1; break;
			case KEY_KP2:	num_code = 2; break;
			case KEY_KP3:	num_code = 3; break;
			case KEY_KP4:	num_code = 4; break;
			case KEY_KP5:	num_code = 5; break;
			case KEY_KP6:	num_code = 6; break;
			case KEY_KP7:	num_code = 7; break;
			case KEY_KP8:	num_code = 8; break;
			case KEY_KP9:	num_code = 9; break;
			default:	num_code = -1; break;
			}

			st->is_alt = num_code != -1;

			if (!st->is_alt) {
				st->key = ev.code;
				do_submit  = true;
			} else
				st->key = st->key * 10 + num_code;
		} else if (st->is_alt) {
			ev.value   = 0;
			do_submit  = true;
			st->is_alt = false;
		} else {
			st->key    = ev.code;
			do_submit  = true;
		}

		break;

	case EV_SYN:
		break;

	case EV_MSC:
		break;

	case EV_REL:
		break;

	default:	;
		printf("%02x\n", ev.type);
	}


	if (do_submit) {
		int		found;

		st->ev.time  = ev.time;
		st->ev.value = ev.value;

		if (ev.value)
			found = fill_key(&st->ev,
					 st->mode |
					 (st->is_alt ? MODE_NUMALT : 0),
					 st->key);
		else
			found = true;

		if (found) {
			ssize_t	l = 0;
			size_t	n = 0;

			if (OUT_MODE == OUT_LIRC) {
				if (st->ev.value > 0) {
					char		buf[128];
					unsigned int	code = (st->ev.type << 16) | (st->ev.code << 0);

					n = sprintf(buf, "%016x %u ", code, st->ev.value-1);
					fill_keyname(buf + n, &st->ev, code);
					strcat(buf, "\n");
					n = strlen(buf);

					l = send(out_fd, buf, n, 0);
				}
			} else {
				n = sizeof st->ev;
				l = write(out_fd, &st->ev, n);
			}

			if (l == 0 && n != 0)
				exit(0);
			if (l < 0)
				exit(1);
			if ((size_t)l != n)
				exit(2);
		} else
			printf("M(%u,%u,%u,%u,%u) %d %d %ld\n",
			       !!(st->mode & MODE_META),
			       !!(st->mode & MODE_CTRL),
			       !!(st->mode & MODE_SHIFT),
			       !!(st->mode & MODE_ALT),
			       !!st->is_alt,
			       st->key, ev.value, (st->mode & MODE_NUMLOCK));

		st->key = 0;
	}

}

int main(int argc, char *argv[])
{
	struct input_state	in[2] = {};
	int			fd_out;
	unsigned long		tmp;

	in[0].fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	in[1].fd = open(argv[2], O_RDONLY | O_NONBLOCK);

	if (OUT_MODE == OUT_LIRC) {
		struct sockaddr_un	addr_out = {
			.sun_family	=  AF_UNIX,
		};
		int		s = socket(AF_UNIX, SOCK_STREAM, 0);
		socklen_t	l = sizeof addr_out;

		strncpy(addr_out.sun_path, argv[3], sizeof addr_out.sun_path);
		bind(s, (void *)&addr_out, sizeof addr_out);
		listen(s, 1);
		fd_out = accept(s, (void *)&addr_out, &l);
		shutdown(fd_out, SHUT_RD);
	} else {
		fd_out = open(argv[3], O_WRONLY);
	}

	tmp = 1;
	ioctl(in[0].fd, EVIOCGRAB, &tmp);
	ioctl(in[1].fd, EVIOCGRAB, &tmp);

	{
		struct input_event	ev;
		size_t			i;
		int			rep[2] = {
			[0] = 400,	/* delay */
			[1] = 200,	/* 1/rate */
		};

		for (i = 0; i < sizeof in / sizeof in[0]; ++i) {
			while (read(in[0].fd, &ev, sizeof ev) > 0)
				;		/* noop */

			ioctl(in[0].fd, EVIOCSREP, rep);
		}
	}

	for (;;) {
		size_t			i;
		struct pollfd		fds[] = {
			{
				.fd = in[0].fd,
				.events = POLLIN
			},

			{
				.fd = in[1].fd,
				.events = POLLIN
			},

			{
				.fd = fd_out,
			},
		};

		poll(fds, sizeof fds/sizeof fds[0], -1);

		for (i = 0; i < sizeof fds/sizeof fds[0]; ++i)
			if (fds[i].revents & (POLLHUP|POLLERR|POLLNVAL))
				exit(1);

		for (i = 0; i < 2; ++i) {
			if (fds[i].revents & POLLIN)
				handle_input(&in[i], fd_out);
		}
	}

}
