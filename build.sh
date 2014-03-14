#!/bin/sh

set -e

[ -d /usr/local/src/llvm/build ] || (echo "Please build LLVM before proceeding."; exit)
[ -d /usr/local/src/stp/install ] || (echo "Please build STP before proceeding."; exit)
[ -d /usr/local/src/klee-uclibc/lib ] || (echo "Please build KLEE-uClibc before proceeding."; exit)

[ -f Makefile.config ] || ./configure --with-llvmsrc=/usr/local/src/llvm --with-llvmobj=/usr/local/src/llvm/build --with-stp=/usr/local/src/stp/bin --with-uclibc=/usr/local/src/klee-uclibc --enable-posix-runtime

make -skj`grep -c processor /proc/cpuinfo` ENABLE_OPTIMIZED=1
