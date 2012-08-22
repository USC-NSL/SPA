#!/bin/sh

make -skj clean
./configure-llvm
make dep
make -skj4
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc pjsua.spa.bc

make -skj clean
./configure-dbg
make dep
make -skj4
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc pjsua.dbg.bc
