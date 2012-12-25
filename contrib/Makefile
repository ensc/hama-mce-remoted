include ../Makefile.common

udevrules_dir = ${sysconfdir}/udev/rules.d

${DESTDIR}${udevrules_dir} ${DESTDIR}${systemd_unitdir}:
	mkdir -p $@

install-contrib: | ${DESTDIR}${udevrules_dir} ${DESTDIR}${systemd_unitdir}
	${INSTALL_DATA} 95-rc.rules ${DESTDIR}${udevrules_dir}/
	${INSTALL_DATA} hama-mce-remoted@.service ${DESTDIR}${systemd_unitdir}/
