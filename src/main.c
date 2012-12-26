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
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>

#include <fcntl.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <linux/uinput.h>
#include <systemd/sd-daemon.h>

#include "keymap-parser.h"

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]))

#define err(_fmt, ...)	fprintf(stderr, SD_ERR _fmt, ## __VA_ARGS__)
#define warn(_fmt, ...)	fprintf(stderr, SD_WARNING _fmt, ## __VA_ARGS__)
#define dbg(_fmt, ...)	fprintf(stderr, SD_DEBUG _fmt, ## __VA_ARGS__)

static struct option const	CMDLINE_OPTIONS[] = {
	{ "keymap",      required_argument, NULL, 'k' },
	{ }
};

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
	int				fd;
	enum { tpKBD, tpMOUSE }	type;
	unsigned long			mode;
	signed int			key;
	bool				is_alt;
	bool				is_raw;

	struct input_event		ev[3];
};

#define M(_meta,_ctrl,_shift,_alt,_num) \
	(((_meta) ? MODE_META : 0) |	\
	 ((_ctrl) ? MODE_CTRL : 0) |	\
	 ((_shift) ? MODE_SHIFT : 0) |  \
	 ((_alt) ? MODE_ALT : 0) |	\
	 ((_num) ? MODE_NUMALT : 0))

#define D(_mask, _key, _code) \
	{ _mask, _key, _code, # _code }

static struct key_definition 		KEY_DEFS[] = {
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
	D( M(0,1,1,0,0), 33,		KEY_FORWARD ),
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

inline static bool test_bit(unsigned int bit, unsigned long const mask[])
{
	unsigned long	m = mask[bit / (sizeof mask[0] * 8)];
	unsigned int	i = bit % (sizeof mask[0] * 8);

	return (m & (1u << i)) != 0u;
}

static bool fill_key(struct input_event *ev, unsigned long mask, unsigned int key)
{
	size_t		i;

	for (i = 0; i < ARRAY_SIZE(KEY_DEFS); ++i) {
		if (KEY_DEFS[i].mask == mask &&
		    KEY_DEFS[i].key  == key) {
			ev[0].type  = EV_MSC;
			ev[0].code  = MSC_SCAN;
			ev[0].value = i;

			ev[1].type  = EV_KEY;
			ev[1].code  = KEY_DEFS[i].code;

			return true;
		}
	}

	return false;
}

static bool send_events(int fd, struct input_event const events[], 
			size_t num_events)
{
	struct input_event const	*ev = &events[0];

	while (num_events > 0) {
		ssize_t	l = write(fd, ev, sizeof *ev);

		if (l < 0 && errno == EINTR) {
			continue;
		} else if (l < 0) {
			err("write(<uinput>): %m");
			break;
		} else if ((size_t)l != sizeof *ev) {
			err("wrote unexpected amount of data: %zd vs. %zu\n",
			    l, sizeof *ev);
			break;
		} else {
			++ev;
			--num_events;
		}
	}

	return num_events == 0;
}

static bool handle_input(struct input_state *st, int out_fd)
{
	struct input_event	ev;
	ssize_t			l;
	unsigned long		mask = 0, value;
	bool			do_submit = false;

	l = read(st->fd, &ev, sizeof ev);
	if (l == 0 || (l < 0 && errno == ENODEV))
		return false;

	if (l < 0) {
		err("read(<event>): %m");
		exit(1);
	}

	if (l != sizeof ev)
		abort();

	switch (ev.type) {
	case EV_KEY:
		st->is_raw = false;

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
		if (st->is_raw &&
		    !send_events(out_fd, &ev, 1))
			exit(1);
		st->is_raw = false;

		break;

	case EV_MSC:
		break;

	case EV_REL:
		if (!send_events(out_fd, &ev, 1))
			exit(1);
		st->is_raw = true;

		break;

	case EV_REP:
		break;

	default:	;
		warn("unsupported event type %02x\n", ev.type);
		break;
	}


	if (do_submit) {
		int		found;

		st->ev[0].time  = ev.time;

		st->ev[1].time  = ev.time;
		st->ev[1].value = ev.value;

		st->ev[2].time  = ev.time;
		st->ev[2].type  = EV_SYN;
		st->ev[2].code  = SYN_REPORT;
		st->ev[2].value = 0;

		if (ev.value)
			found = fill_key(st->ev,
					 st->mode |
					 (st->is_alt ? MODE_NUMALT : 0),
					 st->key);
		else
			found = true;

		if (found) {
			bool		send_scancode = st->ev[1].value == 1;

			if (!send_events(out_fd,
					 send_scancode ? &st->ev[0] : &st->ev[1],
					 send_scancode ? 3 : 2))
				exit(1);
		} else
			warn("unknown code M(%u,%u,%u,%u,%u) %d %d %ld\n",
			     !!(st->mode & MODE_META),
			     !!(st->mode & MODE_CTRL),
			     !!(st->mode & MODE_SHIFT),
			     !!(st->mode & MODE_ALT),
			     !!st->is_alt,
			     st->key, ev.value, (st->mode & MODE_NUMLOCK));

		st->key = 0;
	}

	return true;
}

static bool open_input(struct input_state *st, char const *filename)
{
	static int const	ONE = 1;
	int		fd = open(filename, O_RDONLY);
	unsigned long	events_mask[(EV_CNT +
				     sizeof(unsigned long) * 8 - 1)/
				    (sizeof(unsigned long) * 8)];
	int		rep[2] = {
		[0] = 400,	/* delay */
		[1] = 200,	/* 1/rate */
	};

	if (fd < 0) {
		err("open(<event>): %m");
		return false;
	}

	if (ioctl(fd, EVIOCGRAB, &ONE) < 0 ||
	    ioctl(fd, EVIOCGBIT(0, sizeof events_mask), events_mask) < 0) {
		err("ioctl(<GRAB/NAME>): %m");
		close(fd);
		return false;
	}

	if (ioctl(fd, EVIOCSREP, rep) < 0) {
		warn("failed to set repeat rate: %m");
		/* no error; continue */
	}

	if (test_bit(EV_LED, events_mask))
		st->type = tpKBD;
	else
		st->type = tpMOUSE;



	st->fd = fd;
	return true;
}


static int open_uinput(struct input_state *st)
{
	struct uinput_user_dev		dev_info = {
		.name	= "HAMA emulated device",
		.id	= {
			.bustype	=  BUS_VIRTUAL,
			.vendor		=  0,
			.product	=  0,
			.version	=  0,
		},
	};

	size_t			i;
	int			fd = open("/dev/uinput", O_WRONLY);

	if (fd < 0) {
		err("open(/dev/uinput): %m");
		return -1;
	}

	switch (st->type) {
	case tpKBD:
		strcat(dev_info.name, " (keyboard)");
		dev_info.id.product = 1;
		if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
		    ioctl(fd, UI_SET_EVBIT, EV_MSC) < 0 ||
		    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN) < 0) {
			err("ioctl(<SET_EVBIT>): %m");
			goto err;
		}
		break;

	case tpMOUSE:
		strcat(dev_info.name, " (mouse)");
		dev_info.id.product = 2;
		if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
		    ioctl(fd, UI_SET_EVBIT, EV_REL) < 0 ||
		    ioctl(fd, UI_SET_EVBIT, EV_MSC) < 0 ||
		    ioctl(fd, UI_SET_RELBIT, REL_X) < 0 ||
		    ioctl(fd, UI_SET_RELBIT, REL_Y) < 0 ||
		    ioctl(fd, UI_SET_RELBIT, REL_WHEEL) < 0 ||
		    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN) < 0) {
			err("ioctl(<SET_EVBIT>): %m");
			goto err;
		}
		break;
	}

	for (i = 0; i < ARRAY_SIZE(KEY_DEFS); ++i) {
		if (ioctl(fd, UI_SET_KEYBIT, KEY_DEFS[i].code) < 0) {
			err("ioctl(<SET_KEYBIT>): %m");
			goto err;
		}
	}

	if (write(fd, &dev_info, sizeof dev_info) != sizeof dev_info) {
		err("failed to set uinput dev_info: %m");
		goto err;
	}

	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		err("ioctl(<DEV_CREATE>): %m");
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

static int read_keymap(char const *fname)
{
	int	fds[2];
	pid_t	pid;
	int	rc = EX_OK;

	if (pipe(fds) < 0) {
		err("pipe(): %m");
		return EX_OSERR;
	}

	pid = fork();
	if (pid < 0) {
		err("fork(<keymap-parser): %m");
		close(fds[0]);
		close(fds[1]);
		return EX_OSERR;
	}

	if (pid == 0) {
		char	buf[3 * sizeof(size_t) + 1];

		if (dup2(fds[1], STDOUT_FILENO) < 0) {
			err("dup2(<keymap-parser>): %m");
			_exit(EX_OSERR);
		}

		close(fds[0]);
		close(fds[1]);
		close(0);
		
		sprintf(buf, "%zu", ARRAY_SIZE(KEY_DEFS));
		execlp("hama-keymap-parser",
		       "hama-keymap-parser", fname, buf,
		       (char *)NULL);
		err("execlp(hama-keymap-parser): %m");
		_exit(EX_OSERR);
	} else {
		struct keymap_data_rpc	info;
		int			status;

		close(fds[1]);

		for (;;) {
			ssize_t	l = read(fds[0], &info, sizeof info);

			if (l == 0) {
				break;
			} else if (l < 0) {
				err( "read(<keymap-parser>): %m");
				rc = EX_OSERR;
				break;
			} else if ((size_t)l != sizeof info) {
				err("incomplete data read (%zd vs. %zu)\n",
				    l, sizeof info);
				rc = EX_OSERR;
				break;
			} else if (info.scancode >= ARRAY_SIZE(KEY_DEFS) ||
				   info.keyid >= KEY_MAX) {
				err("inconsistent data (%u, %04x)\n",
				    info.scancode, info.keyid);
				rc = EX_OSERR;
				break;
			} else {
				dbg("mapping %u to %04x\n", info.scancode,
				    info.keyid);
				KEY_DEFS[info.scancode].code = info.keyid;
			}
		}

		close(fds[0]);

		if (waitpid(pid, &status, 0) < 0) {
			err("waitpid(<keymap-parser>): %m");
			if (rc == EX_OK)
				rc = EX_OSERR;
		} else if (!WIFEXITED(status) || WEXITSTATUS(status) != EX_OK) {
			err("keymap parser exited with %x\n", status);
			if (rc != EX_OK)
				;	/* noop */
			else if (WIFEXITED(status))
				rc = WEXITSTATUS(status);
			else
				rc = EX_SOFTWARE;
		}
	}

	return rc;
}

int main(int argc, char *argv[])
{
	struct input_state	in = { };
	int			fd_out;
	char const		*event_filename = NULL;
	int			rc;

	for (;;) {
		int	c;

		c = getopt_long(argc, argv, "e:", CMDLINE_OPTIONS, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'k':
			rc = read_keymap(optarg);
			if (rc != EX_OK)
				return rc;
			break;

		default:
			fprintf(stderr, "unknown option\n");
			return EX_USAGE;
		}
	}

	if (argc < optind + 1) {
		fprintf(stderr, "missing parameters\n");
		return EX_USAGE;
	} else {
		event_filename = argv[optind + 0];
	}

	if (!open_input(&in, event_filename))
		return EX_OSERR;

	fd_out = open_uinput(&in);
	if (fd_out < 0)
		return EX_OSERR;

	while (handle_input(&in, fd_out))
		;
}
