#!/bin/sh

set -e

make -skj clean
CC='llvm-gcc' CFLAGS='-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g' \
CXX='llvm-g++' CXXFLAGS='-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g' \
LD='llvm-ld' LDFLAGS='-disable-opt' \
CPP='llvm-cpp' AR='echo' RANLIB='echo' ./configure \
	--disable-shared \
	--enable-static
make -skj8 || echo
llvm-ld -r -disable-opt -o src/.libs/libspdy.a src/*.o
make -skj8

# make -skj clean
# CC='llvm-gcc' CFLAGS='-DENABLE_SPA -DENABLE_KLEE -DSPA_FIXES -emit-llvm -use-gold-plugin -O0 -g' \
# CXX='llvm-g++' CXXFLAGS='-DENABLE_SPA -DENABLE_KLEE -DSPA_FIXES -emit-llvm -use-gold-plugin -O0 -g' \
# LD='llvm-ld' LDFLAGS='-disable-opt' \
# CPP='llvm-cpp' AR='echo' RANLIB='echo' ./configure \
# 	--disable-shared \
# 	--enable-static
# make -skj8
# 
# make -skj clean
# CFLAGS='-O0 -g' \
# CXXFLAGS='-O0 -g' \
# AR='echo' RANLIB='echo' ./configure \
# 	--disable-shared \
# 	--enable-static
# make -skj8
# 
# make -skj clean
# CFLAGS='-DENABLE_SPA -O0 -g' \
# CXXFLAGS='-DENABLE_SPA -O0 -g' \
# AR='echo' RANLIB='echo' ./configure \
# 	--disable-shared \
# 	--enable-static
# make -skj8
# 
# make -skj clean
# CFLAGS='-DENABLE_SPA -DSPA_FIXES -O0 -g' \
# CXXFLAGS='-DENABLE_SPA -DSPA_FIXES -O0 -g' \
# AR='echo' RANLIB='echo' ./configure \
# --disable-shared \
# --enable-static \
# make -skj8
