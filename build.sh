#!/bin/sh



[ -f Makefile ] && make -s clean

./configure \
	--with-llvm=/usr/local/bin/llvm \
	--with-uclibc=/usr/local/bin/klee-c9-uclibc \
	--enable-posix-runtime \
	--enable-optimized \
	CFLAGS='-g' CXXFLAGS='-g'

make -sk
