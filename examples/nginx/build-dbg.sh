#!/bin/sh

[ -f zlib/Makefile ] || (cd zlib; CFLAGS='-O0 -g' ./configure --static; cd ..)
[ -f Makefile ] || ./configure-dbg

make -skj48
