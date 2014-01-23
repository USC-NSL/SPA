#!/bin/sh

/usr/local/bin/spa/Release+Asserts/bin/spa \
	--stand-alone --libc=uclibc --posix-runtime \
	-max-instruction-time=5 \
	-max-stp-time=5 \
	--path-file=nginx.paths \
	--server objs/nginx.bc \
	2>&1 | tee nginx.paths.log
