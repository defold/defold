ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src

pkgconfig_DATA  = libunibreak.pc
pkgconfigdir    = ${libdir}/pkgconfig

EXTRA_DIST = \
	Doxyfile \
	LICENCE \
	README.md \
	doc \
	tools

.PHONY: doc linebreakdata wordbreakdata graphemebreakdata emojidata

doc:
	cd ${srcdir} && doxygen

linebreakdata:
	cd src && ${MAKE} linebreakdata

wordbreakdata:
	cd src && ${MAKE} wordbreakdata

graphemebreakdata:
	cd src && ${MAKE} graphemebreakdata

eastasianwidthdata:
	cd src && ${MAKE} eastasianwidthdata

indicconjunctbreakdata:
	cd src && ${MAKE} indicconjunctbreakdata

emojidata:
	cd src && ${MAKE} emojidata

update-test:
	cd src && ${MAKE} update-test
