#!/bin/sh

rm -f libosip2.bc libosip2.o

make -skj clean
./configure-llvm
make -skj8
llvm-ld -disable-opt -r src/osip*/*.o -o libosip2.bc

make -skj clean
./configure-dbg
make -skj8
ld -r src/osip*/*.o -o libosip2.o
