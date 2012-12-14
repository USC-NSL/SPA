#!/bin/sh

make -skj clean
./configure-llvm
make -skj8
llvm-ld -r src/osip*/*.o -o libosip2.bc

make -skj clean
./configure-dbg
make -skj8
ld -r src/osip*/*.o -o libosip2.o
