#!/bin/sh

[ -f zlib/Makefile ] || (cd zlib; ./configure-dbg; cd ..)
[ -f Makefile ] || ./configure-dbg

make -skj48
