#!/bin/sh

set -e

rm -f libexosip2.bc libexosip2-fix.bc libexosip2-dbg.o libexosip2-test.o libexosip2-test-fix.o

./configure-llvm
make -skj clean
make -oj`grep -c processor /proc/cpuinfo`
llvm-link src/*.o ../libosip2-3.6.0/libosip2.bc -o libexosip2.bc

./configure-llvm-fix
make -skj clean
make -oj`grep -c processor /proc/cpuinfo`
llvm-link src/*.o ../libosip2-3.6.0/libosip2-fix.bc -o libexosip2-fix.bc

./configure-dbg
make -skj clean
make -oj`grep -c processor /proc/cpuinfo`
ld -r src/*.o ../libosip2-3.6.0/libosip2-dbg.o -o libexosip2-dbg.o

./configure-test
make -skj clean
make -oj`grep -c processor /proc/cpuinfo`
ld -r src/*.o ../libosip2-3.6.0/libosip2-test.o -o libexosip2-test.o

./configure-test-fix
make -skj clean
make -oj`grep -c processor /proc/cpuinfo`
ld -r src/*.o ../libosip2-3.6.0/libosip2-test-fix.o -o libexosip2-test-fix.o
