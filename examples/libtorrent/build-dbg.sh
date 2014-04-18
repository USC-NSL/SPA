#!/bin/sh

set -e

[ -f zlib/Makefile ] || (cd zlib; ./configure-dbg; cd ..)
[ -f Makefile ] || ./configure-dbg

make -okj`grep -c processor /proc/cpuinfo`
