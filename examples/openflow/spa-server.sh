#!/bin/bash
spa --stand-alone --libc=uclibc --posix-runtime --path-file server.paths --server instrumentation/test.bc 2>&1 | tee server.paths.log

