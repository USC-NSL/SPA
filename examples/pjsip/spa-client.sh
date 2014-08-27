#!/bin/sh

rm -rf klee-*

time spa --max-solver-time=1 --max-instruction-time=1 --client pjsua.spa.bc -path-file pjsip-client.paths 2>&1 | tee pjsip-client.paths.log
