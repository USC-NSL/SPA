#!/bin/sh

make -s clean

./configure \
	--with-llvm=/usr/local/bin/llvm \
	--with-uclibc=/usr/local/bin/klee-c9-uclibc \
	CFLAGS='-g' CXXFLAGS='-g'

make -skj48
