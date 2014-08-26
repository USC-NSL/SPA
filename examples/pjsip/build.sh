#!/bin/sh

# Die on failure
set -e

rm -f pjsua.dbg.exe
./configure-dbg
make -skj distclean
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

rm -f pjsua.test.exe
./configure-test
make -skj distclean
make dep
make -k || echo
touch pjmedia/bin/pjmedia-test-x86_64-unknown-linux-gnu
make -k || echo
touch pjsip/bin/pjsip-test-x86_64-unknown-linux-gnu
make -k || echo
mkdir -p pjsip-apps/bin/samples/x86_64-unknown-linux-gnu
touch pjsip-apps/bin/pjsystest-x86_64-unknown-linux-gnu
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/aectest
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/auddemo
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/confsample
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/encdec
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/httpdemo
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/icedemo
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/jbsim
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/latency
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/level
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/mix
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/pcaputil
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/pjsip-perf
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/playfile
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/playsine
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/recfile
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/resampleplay
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/simple_pjsua
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/simpleua
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/siprtp
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/sipstateless
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateful_proxy
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stateless_proxy
make -k || echo
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/stereotest
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/streamutil
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/strerror
touch pjsip-apps/bin/samples/x86_64-unknown-linux-gnu/tonegen
make
cp pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu pjsua.test.exe

rm -f pjsua.spa.bc
./configure-llvm
make -skj distclean
make dep
make -k || echo
llvm-link -o pjsua.spa.bc \
	pjsip-apps/build/output/pjsua-x86_64-unknown-linux-gnu/main.o \
	pjsip-apps/build/output/pjsua-x86_64-unknown-linux-gnu/pjsua_app.o \
	pjsip/lib/libpjsua-x86_64-unknown-linux-gnu.a \
	pjsip/lib/libpjsip-ua-x86_64-unknown-linux-gnu.a \
	pjsip/lib/libpjsip-simple-x86_64-unknown-linux-gnu.a \
	pjsip/lib/libpjsip-x86_64-unknown-linux-gnu.a \
	pjmedia/lib/libpjmedia-codec-x86_64-unknown-linux-gnu.a \
	pjmedia/lib/libpjmedia-x86_64-unknown-linux-gnu.a \
	pjmedia/lib/libpjmedia-audiodev-x86_64-unknown-linux-gnu.a \
	pjnath/lib/libpjnath-x86_64-unknown-linux-gnu.a \
	pjlib-util/lib/libpjlib-util-x86_64-unknown-linux-gnu.a \
	third_party/lib/libresample-x86_64-unknown-linux-gnu.a \
	third_party/lib/libmilenage-x86_64-unknown-linux-gnu.a \
	third_party/lib/libsrtp-x86_64-unknown-linux-gnu.a \
	pjlib/lib/libpj-x86_64-unknown-linux-gnu.a

echo
echo All done.
