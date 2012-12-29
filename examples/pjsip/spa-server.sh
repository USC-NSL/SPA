#!/bin/sh

rm -rf pjsip-apps/bin/klee-*
time spaRR --stand-alone --libc=uclibc --posix-runtime --max-stp-time=1 --max-instruction-time=1 --server pjsua.spa.bc 2>&1 | tee pjsip.server.log
