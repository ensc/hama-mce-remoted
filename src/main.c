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
#include <getopt.h>
#include <sysexits.h>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <systemd/sd-daemon.h>

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]))

static struct option const	CMDLINE_OPTIONS[] = {
	{ "mode",	 required_argument, NULL, 'm' },
	{ "socket",      required_argument, NULL, 's' },
	{ "systemd",     no_argument,       NULL, 'S' },
	{ "events",      required_argument, NULL, 'e' },
	{ }
};

static enum { OUT_LIRC, OUT_DEV, OUT_UINPUT }		OUT_MODE = OUT_LIRC;

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

static struct key_definition const	KEY_DEFS[] = {
	D( 0, KEY_HOMEPAGE,		KEY_HOMEPAGE ),
	D( 0, KEY_SLEEP,		KEY_SLEEP ),

	D( M(0,1,1,0,0), 20,		KEY_YELLOW ),
	D( M(0,1,0,0,0), 50,		KEY_BLUE ),
	D( M(0,1,0,0,0), 23,		KEY_GREEN ),
	D( M(0,1,0,0,0), 18,		KEY_RED ),

	D( M(0,1,0,0,0), 24,		KEY_HOMEPAGE ),
	D( M(0,1,0,0,0), 34,		KEY_PROGRAM ),
	D( M(0,1,0,0,0), 20,		KEY_SCREEN ),
	D( M(0,1,1,0,0), 50,		KEY_DVD ),

	D( M(0,1,1,0,0), 48,		KEY_REWIND ),
	D( M(0,1,1,0,0), 33,		KEY_FASTFORWARD ),
	D( 0, KEY_PREVIOUSSONG,		KEY_PREVIOUSSONG ),
	D( 0, KEY_NEXTSONG,		KEY_NEXTSONG ),

	D( 0, KEY_PLAYPAUSE,		KEY_PLAYPAUSE ),

	D( 0, KEY_STOPCD,		KEY_STOPCD ),
	D( M(0,1,0,0,0), 19,		KEY_RECORD ),

	D( 0, KEY_BACKSPACE,		KEY_BACKSPACE ),
	D( 0, BTN_RIGHT,		KEY_INFO ),

	D( 0, KEY_LEFT,			KEY_LEFT ),
	D( 0, KEY_RIGHT,		KEY_RIGHT ),
	D( 0, KEY_UP,			KEY_UP ),
	D( 0, KEY_DOWN,			KEY_DOWN ),

	D( 0, KEY_ENTER,		KEY_ENTER ),
	D( 0, BTN_LEFT,			BTN_LEFT ),
/*	D( 0, BTN_RIGHT,		BTN_RIGHT ), */

	D( 0, KEY_VOLUMEUP,		KEY_VOLUMEUP ),
	D( 0, KEY_VOLUMEDOWN,		KEY_VOLUMEDOWN ),
	D( 0, KEY_MUTE,			KEY_MUTE ),

	D( 0, KEY_PAGEUP,		KEY_CHANNELUP ),
	D( 0, KEY_PAGEDOWN,		KEY_CHANNELDOWN ),

	D( M(1,0,0,1,0), 28,		BTN_START ),
	D( 0, KEY_KP1,			KEY_KP1 ),
	D( 0, KEY_KP2,			KEY_KP2 ),
	D( 0, KEY_KP3,			KEY_KP3 ),
	D( 0, KEY_KP4,			KEY_KP4 ),
	D( 0, KEY_KP5,			KEY_KP5 ),
	D( 0, KEY_KP6,			KEY_KP6 ),
	D( 0, KEY_KP7,			KEY_KP7 ),
	D( 0, KEY_KP8,			KEY_KP8 ),
	D( 0, KEY_KP9,			KEY_KP9 ),
	D( 0, KEY_KP0,			KEY_KP0 ),

	D( 0, KEY_KPASTERISK,		KEY_KPASTERISK ),
	D( M(0,0,0,0,1), '#',		KEY_KPPLUSMINUS ),
	D( M(0,0,0,1,0), 62,		KEY_CLOSE ),
	D( 0, KEY_ESC,			KEY_ESC ),
};

#undef D
#undef M

static void fill_keyname(char *dst, struct input_event const *ev, unsigned int code)
{
	size_t		i;
	char const	*prefix;

	switch (ev->type) {
	case EV_KEY:
		for (i = 0; i < ARRAY_SIZE(KEY_DEFS); ++i) {
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

	for (i = 0; i < ARRAY_SIZE(KEY_DEFS); ++i) {
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
		fprintf(stderr, SD_WARNING "unsupported event type %02x\n",
			ev.type);
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

			switch (OUT_MODE) {
			case OUT_LIRC:
				if (st->ev.value > 0) {
					char		buf[128];
					unsigned int	code = (st->ev.type << 16) | (st->ev.code << 0);

					n = sprintf(buf, "%016x %u ", code, st->ev.value-1);
					fill_keyname(buf + n, &st->ev, code);
					strcat(buf, "\n");
					n = strlen(buf);
					l = send(out_fd, buf, n, 0);
				}
				break;

			case OUT_UINPUT:
			case OUT_DEV:
				n = sizeof st->ev;
				l = write(out_fd, &st->ev, n);

				if (n == (size_t)l && OUT_MODE == OUT_UINPUT) {
					struct input_event	syn_ev = {
						.type	=  EV_SYN,
						.code	=  SYN_REPORT,
					};

					n = sizeof syn_ev;
					l = write(out_fd, &syn_ev, n);
				}
				break;
			}

			if (l == 0 && n != 0)
				exit(0);
			if (l < 0)
				exit(1);
			if ((size_t)l != n)
				exit(2);
		} else
			fprintf(stderr,
				SD_WARNING "unknown code M(%u,%u,%u,%u,%u) %d %d %ld\n",
				!!(st->mode & MODE_META),
				!!(st->mode & MODE_CTRL),
				!!(st->mode & MODE_SHIFT),
				!!(st->mode & MODE_ALT),
				!!st->is_alt,
				st->key, ev.value, (st->mode & MODE_NUMLOCK));

		st->key = 0;
	}

}

static int accept_connection(int s)
{
	int		res;

	if (listen(s, 1) < 0) {
		perror("listen()");
		return -1;
	}

	res = accept(s, NULL, NULL);
	if (res < 0) {
		perror("accept()");
		return -1;
	}

	if (shutdown(res, SHUT_RD) < 0) {
		perror("shutdown()");
		close(res);
		return -1;
	}

	close(s);
	return res;
}

static int open_uinput(void)
{
	struct uinput_user_dev const	dev_info = {
		.name	= "HAMA emulated device",
		.id	= {
			.bustype	=  BUS_VIRTUAL,
			.vendor		=  0,
			.product	=  0,
			.version	=  0,
		},
	};

	int			fd = open("/dev/uinput", O_WRONLY);
	size_t			i;

	if (fd < 0) {
		perror("open(/dev/uinput");
		return -1;
	}

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
		perror("ioctl(<SET_EVBIT>)");
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(KEY_DEFS); ++i) {
		if (ioctl(fd, UI_SET_KEYBIT, KEY_DEFS[i].code) < 0) {
			perror("ioctl(<SET_KEYBIT>)");
			goto err;
		}
	}

	if (write(fd, &dev_info, sizeof dev_info) != sizeof dev_info) {
		fprintf(stderr, "failed to set uinput dev_info: %m");
		goto err;
	}

	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		perror("ioctl(<DEV_CREATE>)");
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

struct event_file {
	char const	*name;
	char		buf[sizeof "/dev/input/event" + 3 * sizeof(int)];
};

static int fill_event_file(struct event_file *kbd,
			   struct event_file *mouse,
			   char const *opt)
{
	unsigned int	id_kbd;
	unsigned int	id_mouse;
	char		*err_ptr;

	id_kbd = strtoul(opt, &err_ptr, 10);
	if (*err_ptr != ' ')
		return EX_USAGE;

	opt = err_ptr+1;
	id_mouse = strtoul(opt, &err_ptr, 10);
	if (*err_ptr != '\0')
		return EX_USAGE;

	sprintf(kbd->buf, "/dev/input/event%u", id_kbd);
	sprintf(mouse->buf, "/dev/input/event%u", id_mouse);

	kbd->name = kbd->buf;
	mouse->name = mouse->buf;

	return 0;
}

int main(int argc, char *argv[])
{
	struct input_state	in[2] = {};
	int			fd_out;
	unsigned long		tmp;
	char const		*socket_path = NULL;
	bool			use_systemd = false;
	struct event_file	event_kbd = { .name = NULL };
	struct event_file	event_mouse = { .name = NULL };
	int			rc;

	for (;;) {
		int	c;

		c = getopt_long(argc, argv, "m:s:S", CMDLINE_OPTIONS, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			if (strcasecmp(optarg, "dev") == 0)
				OUT_MODE = OUT_DEV;
			else if (strcasecmp(optarg, "uinput") == 0)
				OUT_MODE = OUT_UINPUT;
			else if (strcasecmp(optarg, "lirc") == 0)
				OUT_MODE = OUT_LIRC;
			else {
				fprintf(stderr, "unknown mode '%s'\n",
					optarg);
				return EX_USAGE;
			}
			break;

		case 's':
			socket_path = optarg;
			break;

		case 'S':
			use_systemd = true;
			break;

		case 'e':
			rc = fill_event_file(&event_kbd, &event_mouse,
					     optarg);
			if (rc)
				return rc;
			break;

		default:
			fprintf(stderr, "unknown option\n");
			return EX_USAGE;
		}
	}

	if (event_kbd.name != NULL) {
		;			/* noop */
	} else if (argc < optind + 2) {
		fprintf(stderr, "missing parameters\n");
		return EX_USAGE;
	} else {
		event_kbd.name   = argv[optind + 0];
		event_mouse.name = argv[optind + 1];
	}

	if (socket_path && use_systemd) {
		fprintf(stderr, "both socket path specified and systemd enabled\n");
		return EX_USAGE;
	}

	if (!socket_path && !use_systemd && OUT_MODE != OUT_UINPUT) {
		fprintf(stderr, "neither socket path specified, nor systemd enabled\n");
		return EX_USAGE;
	}

	if (use_systemd && OUT_MODE == OUT_UINPUT) {
		fprintf(stderr, "can not use uinput and systemd mode together\n");
		return EX_USAGE;
	}

	in[0].fd = open(event_kbd.name,   O_RDONLY | O_NONBLOCK);
	in[1].fd = open(event_mouse.name, O_RDONLY | O_NONBLOCK);

	if (!use_systemd) {
		fd_out = -1;
	} else {
		if (sd_listen_fds(0) <= 0) {
			fprintf(stderr, "systemd not correctly initialized\n");
			return EX_USAGE;
		}

		fd_out = accept_connection(SD_LISTEN_FDS_START + 0);
	}

	if (fd_out >= 0)
		;			/* noop */
	else {
		switch (OUT_MODE) {
		case OUT_LIRC: {
			struct sockaddr_un	addr_out = {
				.sun_family	=  AF_UNIX,
			};
			int		s = socket(AF_UNIX, SOCK_STREAM, 0);

			strncpy(addr_out.sun_path, socket_path, sizeof addr_out.sun_path);
			bind(s, (void *)&addr_out, sizeof addr_out);

			fd_out = accept_connection(s);
			break;
		}

		case OUT_DEV:
			fd_out = open(socket_path, O_WRONLY);
			break;

		case OUT_UINPUT:
			fd_out = open_uinput();
			break;
		}
	}

	if (fd_out < 0)
		return EX_OSERR;

	tmp = 1;
	if (ioctl(in[0].fd, EVIOCGRAB, &tmp) < 0 ||
	    ioctl(in[1].fd, EVIOCGRAB, &tmp) < 0) {
		perror("ioctl(..., EVICGRAB)");
		return EX_IOERR;
	}

	{
		struct input_event	ev;
		size_t			i;
		int			rep[2] = {
			[0] = 400,	/* delay */
			[1] = 200,	/* 1/rate */
		};

		for (i = 0; i < ARRAY_SIZE(in); ++i) {
			while (read(in[i].fd, &ev, sizeof ev) > 0)
				;		/* noop */

			ioctl(in[i].fd, EVIOCSREP, rep);
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

		poll(fds, ARRAY_SIZE(fds), -1);

		for (i = 0; i < ARRAY_SIZE(fds); ++i)
			if (fds[i].revents & (POLLHUP|POLLERR|POLLNVAL))
				exit(1);

		for (i = 0; i < 2; ++i) {
			if (fds[i].revents & POLLIN)
				handle_input(&in[i], fd_out);
		}
	}

}
