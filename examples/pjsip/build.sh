#!/bin/sh

make -skj distclean
./configure-dbg
make -skj distclean
make dep
make -skj8
make -sk
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc pjsua.dbg.bc

make -skj distclean
./configure-llvm
make -skj distclean
make dep
make -skj8
make -sk
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc pjsua.spa.bc
