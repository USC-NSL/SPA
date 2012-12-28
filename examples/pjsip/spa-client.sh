#!/bin/sh

time spa --stand-alone --libc=uclibc --posix-runtime --max-stp-time=1 --max-instruction-time=1 --client pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc 2>&1 | tee client.log
