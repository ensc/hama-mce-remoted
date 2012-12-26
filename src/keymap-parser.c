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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <ctype.h>
#include <string.h>
#include <sysexits.h>

#include <linux/input.h>
#include "keymap-parser.h"

#include "gen-keymap.h"

int main(int argc, char *argv[])
{
	FILE		*f;
	char		*line = NULL;
	size_t		line_sz = 0;
	unsigned int	line_num;
	size_t		max_scancode;
	char const	*fname;
	bool		ignore_missing = false;

	if (argc != 3) {
		fprintf(stderr, "bad parameters\n");
		return EX_USAGE;
	}

	fname = argv[1];
	if (fname[0] == '-') {
		ignore_missing = true;
		++fname;
	}

	f = fopen(fname, "r");
	if (f) {
		;			/* noop */
	} else if (ignore_missing) {
		return EX_OK;
	} else {
		fprintf(stderr, "failed to open keymap file '%s': %m\n",
			fname);
		return EX_NOINPUT;
	}

	max_scancode = strtoul(argv[2], NULL, 10);

	for (line_num = 1;; ++line_num) {
		ssize_t		l = getline(&line, &line_sz, f);
		char		*ptr;
		char		*err_ptr;
		unsigned int	scancode;
		unsigned int	keyid;
		struct keymap_data_rpc		info = { };

		if (l < 0 && feof(f)) {
			break;
		} else if (l < 0) {
			fprintf(stderr,
				"failed to read from keymap file '%s': %m\n",
				fname);
			return EX_IOERR;
		}

		ptr = strchr(line, '#');
		if (ptr)
			*ptr = '\0';

		l = strlen(line);
		while (l > 0 && (isspace(line[l-1]) || 
				 line[l-1] == '\n' || line[l-1] == '\r'))
			--l;
		line[l] = '\0';

		ptr = line;
		while (isspace(*ptr))
			++ptr;

		scancode = strtoul(ptr, &err_ptr, 0);
		if (err_ptr == ptr || !isspace(*err_ptr)) {
			fprintf(stderr, "%s:%u invalid scancode\n",
				fname, line_num);
			continue;
		}

		ptr = err_ptr;
		while (isspace(*ptr) && *ptr != '\0')
			++ptr;

		if (*ptr == '$') {
			++ptr;
			keyid = strtoul(ptr, &err_ptr, 0);
			if (err_ptr == ptr || (!isspace(*err_ptr) && *err_ptr)) {
				fprintf(stderr, "%s:%u invalid keyid\n",
					fname, line_num);
				continue;
			}
		} else {
			struct keymap_def const	*def = in_word_set(ptr,
								   line + l - ptr);

			if (!def) {
				fprintf(stderr, "%s:%u unknown keyname '%s'",
					fname, line_num, ptr);
				continue;
			}

			keyid = def->num;
		}

		if (scancode >= max_scancode) {
			fprintf(stderr, "%s:%u scancode %u out of range\n",
				fname, line_num, scancode);
			continue;
		}

		if (keyid >= KEY_MAX) {
			fprintf(stderr, "%s:%u unsupported keyid %u\n",
				fname, line_num, scancode);
			continue;
		}

		info.scancode = scancode;
		info.keyid    = keyid;

		if (fwrite(&info, sizeof info, 1, stdout) != 1)
			return EX_OSERR;
	}

	fclose(f);

	return EX_OK;
}
