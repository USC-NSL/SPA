#!/bin/bash
spa --stand-alone --libc=uclibc --posix-runtime --path-file client.paths --client instrumentation/test-client.bc 2>&1 | tee client.paths.log

