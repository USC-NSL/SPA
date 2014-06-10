#!/bin/sh

set -e

rm -f libspdylay.bc libspdylay-fixed.bc libspdylay-dbg.o libspdylay-test.o libspdylay-test-fixed.o

make -skj distclean || echo
CC='clang' CFLAGS='-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g' \
CXX='clang++' CXXFLAGS='-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g' \
LD='llvm-ld' LDFLAGS='-disable-opt' \
CPP='clang -E' AR='llvm-ar' RANLIB='llvm-ranlib' ./configure \
	--disable-shared \
	--enable-static \
	--disable-xmltest --disable-largefile
make -skj8
llvm-link lib/*.o -o libspdylay.bc

# make -skj clean
# CC='llvm-gcc' CFLAGS='-DENABLE_SPA -DENABLE_KLEE -DSPA_FIXES -emit-llvm -use-gold-plugin -O0 -g' \
# CXX='llvm-g++' CXXFLAGS='-DENABLE_SPA -DENABLE_KLEE -DSPA_FIXES -emit-llvm -use-gold-plugin -O0 -g' \
# LD='llvm-ld' LDFLAGS='-disable-opt' \
# CPP='llvm-cpp' AR='echo' RANLIB='echo' ./configure \
# 	--disable-shared \
# 	--enable-static
# make -skj8
# llvm-ld -disable-opt -r lib/*.o -o libspdylay-fixed.bc

make -skj clean
CFLAGS='-DENABLE_SPA -O0 -g' \
CXXFLAGS='-DENABLE_SPA -O0 -g' ./configure \
	--disable-shared \
	--enable-static
make -skj8
ld -r lib/*.o -o libspdylay-test.o

# make -skj clean
# CFLAGS='-DENABLE_SPA -DSPA_FIXES -O0 -g' \
# CXXFLAGS='-DENABLE_SPA -DSPA_FIXES -O0 -g' \
# AR='echo' RANLIB='echo' ./configure \
# --disable-shared \
# --enable-static \
# make -skj8
# ld -r lib/*.o -o libspdylay-test-fixed.o

make -skj clean
CFLAGS='-O0 -g' \
CXXFLAGS='-O0 -g' ./configure \
	--disable-shared \
	--enable-static
make -skj8
ld -r lib/*.o -o libspdylay-dbg.o

make -kf Makefile.spa
