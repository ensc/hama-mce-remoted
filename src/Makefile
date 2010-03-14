include ../Makefile.common

hama-mce-remoted:	main.c
	${CC} ${CFLAGS} $^ -o $@

install:		hama-mce-remoted
	${MKDIR} ${DESTDIR}${sbindir}
	${INSTALL_PROG} hama-mce-remoted ${DESTDIR}${sbindir}/
