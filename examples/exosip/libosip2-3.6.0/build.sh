#!/bin/sh

make -skj4
llvm-ld -r src/osip*/*.o -o libosip2.o
