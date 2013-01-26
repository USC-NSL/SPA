#!/bin/sh

rm -f libexosip2.bc libexosip2-dbg.o libexosip2-test.o

make -skj clean
./configure-llvm
make -skj8
llvm-ld -disable-opt -r src/*.o ../libosip2-3.6.0/libosip2.bc -o libexosip2.bc

make -skj clean
./configure-dbg
make -skj8
ld -r src/*.o ../libosip2-3.6.0/libosip2.o -o libexosip2-dbg.o

make -skj clean
./configure-test
make -skj8
ld -r src/*.o ../libosip2-3.6.0/libosip2.o -o libexosip2-test.o
