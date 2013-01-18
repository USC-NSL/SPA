#!/bin/bash
rm -f server.paths
spa --stand-alone --libc=uclibc --posix-runtime --path-file server.paths --server instrumentation/test-server.bc 2>&1 | tee server.paths.log

