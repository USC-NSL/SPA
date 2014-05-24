#!/bin/sh

set -e

[ -f src/Makefile ] || ./configure-dbg
[ -f Makefile ] || ./configure-dbg

make -okj4
