#!/bin/sh

make -skj clean
./configure-llvm
make -skj4
llvm-ld -r src/osip*/*.o -o libosip2.bc

make -skj clean
./configure-dbg
make -skj4
ld -r src/osip*/*.o -o libosip2.o
