#!/bin/sh

# Die on failure
set -e

make -skj distclean
rm -f pjsua.dbg.exe
./configure-dbg
make dep
make -k || echo
touch pjmedia/bin/pjmedia-test-x86_64-unknown-linux-gnu
make -k || echo
touch pjsip/bin/pjsip-test-x86_64-unknown-linux-gnu
make -k || echo
mkdir -p pjsip-apps/bin/samples/x86_64-unknown-linux-gnu
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/pjsip-perf
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/simpleua
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/siprtp
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/sipstateless
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateful_proxy
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateless_proxy
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/playsine
make
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu pjsua.dbg.exe

make -skj distclean
rm -f pjsua.test.exe
./configure-test
make dep
make -k || echo
touch pjmedia/bin/pjmedia-test-x86_64-unknown-linux-gnu
make -k || echo
touch pjsip/bin/pjsip-test-x86_64-unknown-linux-gnu
make -k || echo
mkdir -p pjsip-apps/bin/samples/x86_64-unknown-linux-gnu
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/pjsip-perf
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/simpleua
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/siprtp
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/sipstateless
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateful_proxy
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateless_proxy
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/playsine
make
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu pjsua.test.exe

make -skj distclean
rm -f pjsua.spa.bc
./configure-llvm
make dep
make -k || echo
touch pjlib/bin/pjlib-test-x86_64-unknown-linux-gnu
make
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu.bc pjsua.spa.bc
