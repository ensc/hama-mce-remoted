include ../Makefile.common

AM_CFLAGS = -ffunction-sections
AM_LDFLAGS = -Wl,--gc-sections

sbin_PROGRAMS =	hama-mce-remoted

systemd_SOURCES = systemd/sd-daemon.h systemd/sd-daemon.c

all:	${sbin_PROGRAMS}

clean:
	rm -f ${sbin_PROGRAMS}

install:		${sbin_PROGRAMS}
	${MKDIR} ${DESTDIR}${sbindir}
	${INSTALL_PROG} ${sbin_PROGRAMS} ${DESTDIR}${sbindir}/

hama-mce-remoted:	main.c ${systemd_SOURCES}
	${CC} ${AM_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $^ -o $@
