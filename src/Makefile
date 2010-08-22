include ../Makefile.common

sbin_PROGRAMS =	hama-mce-remoted

all:	${sbin_PROGRAMS}

clean:
	rm -f ${sbin_PROGRAMS}

install:		${sbin_PROGRAMS}
	${MKDIR} ${DESTDIR}${sbindir}
	${INSTALL_PROG} ${sbin_PROGRAMS} ${DESTDIR}${sbindir}/

hama-mce-remoted:	main.c
	${CC} ${CFLAGS} $(LDFLAGS) $^ -o $@
