#!/bin/sh

make -s clean

./configure \
	--with-llvm=/usr/local/bin/llvm \
	--with-uclibc=/usr/local/bin/klee-c9-uclibc

make -skj48
