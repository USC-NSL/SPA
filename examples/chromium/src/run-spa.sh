#!/bin/sh

/usr/local/bin/spa/Release+Asserts/bin/spa \
	--stand-alone --libc=uclibc --posix-runtime \
	--path-file=fetch_client.paths \
	--client out/Debug/fetch_client \
	2>&1 | tee fetch_client.paths.log
