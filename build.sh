#!/bin/sh

set -e

[ -d /usr/local/src/llvm/build ] || (echo "Please build LLVM before proceeding."; exit 1)
[ -d /usr/local/src/stp/bin ] || (echo "Please build STP before proceeding."; exit 1)
[ -d /usr/local/src/klee-uclibc/lib ] || (echo "Please build KLEE-uClibc before proceeding."; exit 1)

[ -f Makefile.config ] || CXXFLAGS='-std=c++11 -g' CFLAGS='-g' ./configure --with-llvmsrc=/usr/local/src/llvm --with-llvmobj=/usr/local/src/llvm/build --with-stp=/usr/local/src/stp/bin --with-uclibc=/usr/local/src/klee-uclibc --enable-posix-runtime

make -okj`grep -c processor /proc/cpuinfo` ENABLE_OPTIMIZED=1
