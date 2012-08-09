#!/bin/sh

make -skj clean
./configure-llvm
make -skj4
llvm-ld -r src/*.o ../libosip2-3.6.0/libosip2.bc -o libexosip2.bc

make -skj clean
./configure-dbg
make -skj4
ld -r src/*.o ../libosip2-3.6.0/libosip2.o -o libexosip2.o

