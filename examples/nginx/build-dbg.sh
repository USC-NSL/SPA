#!/bin/sh

[ -f Makefile ] || ./configure-dbg

make -skj48
