srcdir = .
VPATH = $(abspath ${srcdir})

include ${srcdir}/../Makefile.common

WARN_OPTS = -Wall -W -Wno-missing-field-initializers
AM_CFLAGS = -std=gnu99 -Wall ${WARN_OPTS}
AM_LDFLAGS =

udevdir = ${prefix}/lib/udev

sbin_PROGRAMS =	hama-mce-remoted
udev_PROGRAMS = hamaid

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
	${INSTALL_PROG} ${udev_PROGRAMS} ${DESTDIR}${udevdir}/

hama-mce-remoted:	main.c
	${CC} ${AM_CFLAGS} ${sytemd_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $^ -o $@ ${systemd_LIBS}

hamaid:			udev-hamaid.c
	${CC} ${AM_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $^ -o $@
