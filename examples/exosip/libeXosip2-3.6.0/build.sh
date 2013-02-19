#!/bin/sh

set -e

rm -f libexosip2.bc libexosip2-fix.bc libexosip2-dbg.o libexosip2-test.o libexosip2-test-fix.o

make -skj clean
./configure-llvm
make -skj8
llvm-ld -disable-opt -r src/*.o ../libosip2-3.6.0/libosip2.bc -o libexosip2.bc

make -skj clean
./configure-llvm-fix
make -skj8
llvm-ld -disable-opt -r src/*.o ../libosip2-3.6.0/libosip2-fix.bc -o libexosip2-fix.bc

make -skj clean
./configure-dbg
make -skj8
ld -r src/*.o ../libosip2-3.6.0/libosip2-dbg.o -o libexosip2-dbg.o

make -skj clean
./configure-test
make -skj8
ld -r src/*.o ../libosip2-3.6.0/libosip2-test.o -o libexosip2-test.o

make -skj clean
./configure-test-fix
make -skj8
ld -r src/*.o ../libosip2-3.6.0/libosip2-test-fix.o -o libexosip2-test-fix.o
