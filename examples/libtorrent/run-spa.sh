#!/bin/sh

../../Release+Asserts/bin/spa \
	-max-instruction-time=5 \
	-max-solver-time=5 \
	--path-file=nginx.paths \
	--server objs/nginx.bc \
	2>&1 | tee nginx.paths.log
