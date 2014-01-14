#!/bin/sh

/usr/local/bin/spa/Release+Asserts/bin/spa \
	--stand-alone --libc=uclibc --posix-runtime \
	--path-file=nginx.paths \
	--server objs/nginx.bc \
	2>&1 | tee nginx.paths.log
