#!/bin/sh

set -e

rm -f libosip2.bc libosip2-fix.bc libosip2-dbg.o libosip2-test.o libosip2-test-fix.o

make -skj clean
./configure-llvm
make -skj8 || echo
llvm-ld -disable-opt -r src/osip*/*.o -o libosip2.bc

make -skj clean
./configure-llvm-fix
make -skj8 || echo
llvm-ld -disable-opt -r src/osip*/*.o -o libosip2-fix.bc

make -skj clean
./configure-dbg
make -skj8 || echo
ld -r src/osip*/*.o -o libosip2-dbg.o

make -skj clean
./configure-test
make -skj8 || echo
ld -r src/osip*/*.o -o libosip2-test.o

make -skj clean
./configure-test-fix
make -skj8 || echo
ld -r src/osip*/*.o -o libosip2-test-fix.o
