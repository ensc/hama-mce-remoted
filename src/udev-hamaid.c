/*	--*- c -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
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

#include <string.h>
#include <stdio.h>
#include <sysexits.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

static int find_input_event(DIR *dir)
{
	int		id = -1;

	while (id < 0) {
		struct dirent	*ent = readdir(dir);
		char		*errptr;
		unsigned int	num;
		struct stat	st;

		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;


		num = strtoul(ent->d_name+5, &errptr, 10);
		if (*errptr || errptr == ent->d_name+5)
			continue;

		if (fstatat(dirfd(dir), ent->d_name, &st, 0) < 0 ||
		    !S_ISDIR(st.st_mode))
			continue;

		id = num;
	}

	return id;
}

static int find_event(int fd_dir, char const *base, char *ptr, char idx)
{
	DIR		*dir = NULL;
	int		fd;
	int		id = -1;

	sprintf(ptr, "%c/input", idx);
	fd = openat(fd_dir, base, O_DIRECTORY|O_RDONLY);

	if (fd < 0)
		return -1;

	dir = fdopendir(fd);
	if (!dir) {
		close(fd);
		return -1;
	}

	while (id < 0) {
		struct dirent	*ent = readdir(dir);
		int		fd_input;
		DIR		*dir_input;

		if (!ent)
			break;

		if (strncmp(ent->d_name, "input", 5) != 0)
			continue;

		fd_input = openat(dirfd(dir), ent->d_name, O_DIRECTORY|O_RDONLY);
		if (fd_input < 0)
			continue;

		dir_input = fdopendir(fd_input);
		if (!dir_input) {
			close(fd_input);
			continue;
		}

		id = find_input_event(dir_input);
		closedir(dir_input);
	}

	closedir(dir);

	return id;
}

static int find_files(int fd_dir, char *base)
{
	char	*ptr = base + strlen(base);
	int	ev_num[0];

	ev_num[0] = find_event(fd_dir, base, ptr, '0');
	ev_num[1] = find_event(fd_dir, base, ptr, '1');

	if (ev_num[0] < 0 || ev_num[1] < 0)
		return EX_UNAVAILABLE;

	printf("HAMA_EVENTS=%u\\x20%u\n", ev_num[0], ev_num[1]);
	return EX_OK;
}

int main(int argc, char *argv[])
{
	char		*path = NULL;
	char const	*dev_path = getenv("DEVPATH");
	char		*ptr;
	size_t		idx = 4;
	char		c;
	int		fd;

	/* make sure that enough space is allocated by appending a dummy
	 * "input-" which is used in find_event() above */
	if (asprintf(&path, "/sys%sinput-", getenv("DEVPATH")) < 0)
		return EX_OSERR;

	/* path = /sys/devices/.../2-1.2.2/2-1.2.2:1.1/input/input7/event7 */

	ptr = path + strlen(path);
	while (idx > 0 && ptr > path) {
		--ptr;
		if (*ptr == '/') {
			--idx;
			*ptr = '\0';
		}
	}

	/* path = /sys/devices/.../2-1.2.2
	   ptr  = \02-1.2.2:1.1 */

	if (idx != 0 || ptr == path || strlen(ptr+1) < 2)
		return EX_UNAVAILABLE;

	++ptr;
	ptr[strlen(ptr) - 1] = '\0';
	if (ptr[strlen(ptr) - 1] != '.')
		return EX_UNAVAILABLE;

	/* path = /sys/devices/.../2-1.2.2
	   ptr  = \02-1.2.2:1. */

	fd = open(path, O_DIRECTORY|O_RDONLY);
	if (fd < 0)
		return EX_UNAVAILABLE;

	return find_files(fd, ptr);
}
