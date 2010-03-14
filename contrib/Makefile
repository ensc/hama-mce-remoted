include ../Makefile.common

udevrules_dir = ${sysconfdir}/udev/rules.d
upstart_dir   = ${sysconfdir}/init/services
files_dir     = ${sysconfdir}/files.d

install-contrib:
	${MKDIR} ${DESTDIR}${udevrules_dir}/
	${INSTALL_DATA} 95-rc.rules ${DESTDIR}${udevrules_dir}/

	${MKDIR} ${DESTDIR}${upstart_dir}
	${INSTALL_DATA} hama-mce-remoted.conf ${DESTDIR}${upstart_dir}/

	${MKDIR} ${DESTDIR}${files_dir}
	${INSTALL_DATA} 02-hama-mce.txt ${DESTDIR}${files_dir}/
