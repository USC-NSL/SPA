all:	depend compile

compile:
	cd src && ${MAKE}

depend:
	cd src && ${MAKE} depend

clean:
	cd src && ${MAKE} clean

distclean:
	cd src && ${MAKE} distclean
	rm -f config.log config.cache config.status
	@cp Makefile.default Makefile
	@cp include/config.h.default include/config.h
