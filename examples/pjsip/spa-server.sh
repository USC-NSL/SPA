#!/bin/sh

rm -rf pjsip-apps/bin/klee-*
time spa --stand-alone --libc=uclibc --posix-runtime --max-stp-time=1 --max-instruction-time=1 -init-values pjsip-options.in --server pjsua.spa.bc 2>&1 | tee pjsip.server.log
