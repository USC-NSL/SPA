#!/bin/sh

set -e

BUILD_OBJS="\
	benc.o \
	bts.o \
	types.o \
	random.o \
	strbuf.o \
	stream.o \
	peer.o \
	segmenter.o \
	util.o \
	bitset.o \
	context.o \
	bterror.o \
    udpproto.o \
	peerexchange.o"

FLAGS="-emit-llvm -I../include -DENABLE_SPA -DENABLE_KLEE -I../../../include -DVERSION=1.06"

LIBFLAGS="-lbt -lcurl  -lssl -lcrypto -lssl `curl-config --libs` -lm"

make clean
[ -f src/spaget.o ] && rm src/spaget.o
[ -f src/spaget.bc ] && rm src/spaget.bc

./configure-llvm

cd src
make  -kj4 $BUILD_OBJS
clang -c btget.c -o spaget.o $FLAGS
#-f src/Makefile
echo "Linking libbt"
llvm-link -o libbt.bc $BUILD_OBJS
echo "Linking client with libbt"
llvm-link -o spaget.bc spaget.o libbt.bc
echo "Success"
cd ..