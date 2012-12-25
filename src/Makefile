include ../Makefile.common

AM_CFLAGS = -ffunction-sections
AM_LDFLAGS = -Wl,--gc-sections

sbin_PROGRAMS =	hama-mce-remoted

PKG_CONFIG = pkg-config
systemd_CFLAGS = $(shell $(PKG_CONFIG) --cflags libsystemd-daemon)
systemd_LIBS   = $(shell $(PKG_CONFIG) --libs libsystemd-daemon)

all:	${sbin_PROGRAMS}

clean:
	rm -f ${sbin_PROGRAMS}

install:		${sbin_PROGRAMS}
	${MKDIR} ${DESTDIR}${sbindir}
	${INSTALL_PROG} ${sbin_PROGRAMS} ${DESTDIR}${sbindir}/

hama-mce-remoted:	main.c
	${CC} ${AM_CFLAGS} ${sytemd_CFLAGS} ${CFLAGS} ${AM_LDFLAGS} $(LDFLAGS) $^ -o $@ ${systemd_LIBS}
