include ../Makefile.common

udevrules_dir = ${sysconfdir}/udev/rules.d
upstart_dir   = ${sysconfdir}/init/services
files_dir     = ${sysconfdir}/files.d

install-contrib:
	install -d -m 0755 ${DESTDIR}${udevrules_dir}/
	install -p -m 0644 95-rc.rules ${DESTDIR}${udevrules_dir}/

	install -d -m 0755 ${DESTDIR}${upstart_dir}
	install -p -m 0644 hama-mce-remoted.conf ${DESTDIR}${upstart_dir}/

	install -d -m 0755 ${DESTDIR}${files_dir}
	install -p -m 0644 02-hama-mce.txt ${DESTDIR}${files_dir}/
