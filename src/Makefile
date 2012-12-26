srcdir = .
VPATH = $(abspath ${srcdir})

include ${srcdir}/../Makefile.common

WARN_OPTS = -Wall -W -Wno-missing-field-initializers
AM_CFLAGS = -std=gnu99 -Wall -I. ${WARN_OPTS}
AM_LDFLAGS =
GPERF_FLAGS = -L ANSI-C --ignore-case

sbin_PROGRAMS =	hama-mce-remoted hama-keymap-parser

PKG_CONFIG = pkg-config
systemd_CFLAGS = $(shell $(PKG_CONFIG) --cflags libsystemd-daemon)
systemd_LIBS   = $(shell $(PKG_CONFIG) --libs libsystemd-daemon)

all:	${sbin_PROGRAMS} ${udev_PROGRAMS}

clean:
	rm -f ${sbin_PROGRAMS}

${DESTDIR}${sbindir} ${DESTDIR}${udevdir}:
	${MKDIR} '$@'

install:		${sbin_PROGRAMS} ${udev_PROGRAMS} | ${DESTDIR}${sbindir} ${DESTDIR}${udevdir}
	${INSTALL_PROG} ${sbin_PROGRAMS} ${DESTDIR}${sbindir}/

hama-mce-remoted:	main.c keymap-parser.h
	${CC} ${AM_CFLAGS} ${sytemd_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $(filter %.c,$^) -o $@ ${systemd_LIBS}

hama-keymap-parser:	keymap-parser.c keymap-parser.h gen-keymap.h
	${CC} ${AM_CFLAGS} ${sytemd_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $(filter %.c,$^) -o $@ ${systemd_LIBS}

KEY_BLACKLIST = RESERVED\|MAX\|CNT

KEYMAP_SED = \
  -e '/^\#define KEY_\($(KEY_BLACKLIST)\)$$/d' \
  -e '/^\#define KEY_/{' \
  -e 's/^\#define KEY_\([A-Za-z0-9_]\+\)[[:space:]]*$$/\L\1\E,KEY_\1/p' \
  -e '};d'

gen-keymap.h:	gen-keymap.perf
	$(GPERF) $(GPERF_FLAGS) -t $< --output-file=$@

gen-keymap.perf:	Makefile
	rm -f $@ $@.tmp
	@echo '%readonly-tables' > $@.tmp
	@echo 'struct keymap_def { char *name; unsigned int num; };' >> $@.tmp
	@echo '%%' >> $@.tmp
	@${CC} ${AM_CFLAGS} ${CFLAGS} -imacros 'linux/input.h' -E -dN - </dev/null | \
		$(SED) $(KEYMAP_SED) | sort -n >>$@.tmp
	@mv $@.tmp $@
