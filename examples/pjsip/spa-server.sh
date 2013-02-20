#!/bin/sh

rm -rf klee-*
#time spa --stand-alone --libc=uclibc --posix-runtime --max-stp-time=1 --max-instruction-time=1 -init-values pjsip-options-client.in -path-file pjsip-options-client.paths --server pjsua.spa.bc 2>&1 | tee pjsip.server.log
time spa --stand-alone --libc=uclibc --posix-runtime --max-stp-time=1 --max-instruction-time=1 -path-file pjsip.paths --server pjsua.spa.bc 2>&1 | tee pjsip.paths.log
