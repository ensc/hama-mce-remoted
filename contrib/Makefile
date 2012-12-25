include ../Makefile.common

udevrules_dir = ${sysconfdir}/udev/rules.d

install-contrib:
	${MKDIR} ${DESTDIR}${udevrules_dir}/
	${INSTALL_DATA} 95-rc.rules ${DESTDIR}${udevrules_dir}/
