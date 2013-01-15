LD=llvm-ld -disable-opt
CXX=llvm-g++
AR=echo
RANLIB=echo
CFLAGS=-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g
CXX=llvm-g++
CXXFLAGS=-DENABLE_SPA -DENABLE_KLEE -emit-llvm -use-gold-plugin -O0 -g
CC=llvm-gcc
CPP=llvm-cpp

LIB_OBJ=lib/dhcp-client.o lib/util.o lib/vlog.o lib/vconn-unix.o lib/dynamic-string.o lib/fatal-signal.o lib/dirs.o lib/hash.o lib/backtrace.o lib/learning-switch.o lib/svec.o lib/bitmap.o lib/random.o lib/port-array.o lib/vconn.o lib/ofpbuf.o lib/vconn-stream.o lib/fault.o lib/vconn-netlink.o lib/stp.o lib/tag.o lib/process.o lib/flow.o lib/leak-checker.o lib/dhcp.o lib/socket-util.o lib/queue.o lib/hmap.o lib/vconn-tcp.o lib/poll-loop.o lib/timeval.o lib/ofp-print.o lib/list.o lib/daemon.o lib/pcap.o lib/dpif.o lib/signals.o lib/command-line.o lib/rconn.o lib/netdev.o lib/ofpstat.o lib/shash.o lib/mac-learning.o lib/vlog-socket.o lib/csum.o lib/netlink.o

BIN=secchan/ofprotocol controller/controller utilities/vlogconf utilities/dpctl utilities/ofp-discover utilities/ofp-kill udatapath/ofdatapath tests/test-flows tests/test-hmap tests/test-list tests/test-dhcp-client tests/test-stp instrumentation/test-server instrumentation/test-client
.PHONY: all clean

all: ${BIN}

clean:
	rm -f ${BIN} */*.bc instrumentation/test.o

secchan/ofprotocol: secchan/discovery.o secchan/emerg-flow.o secchan/fail-open.o secchan/failover.o secchan/in-band.o secchan/port-watcher.o secchan/protocol-stat.o secchan/ratelimit.o secchan/secchan.o secchan/status.o secchan/stp-secchan.o ${LIB_OBJ}
	${LD}  -o $@ $^

controller/controller: controller/controller.o ${LIB_OBJ}
	${LD}  -o $@ $^

utilities/vlogconf: utilities/vlogconf.o ${LIB_OBJ}
	${LD}  -o $@ $^

utilities/dpctl: utilities/dpctl.o ${LIB_OBJ}
	${LD}  -o $@ $^

utilities/ofp-discover: utilities/ofp-discover.o  ${LIB_OBJ}
	${LD}  -o $@ $^

utilities/ofp-kill: utilities/ofp-kill.o  ${LIB_OBJ}
	${LD}  -o $@ $^

udatapath/ofdatapath: udatapath/udatapath_ofdatapath-chain.o udatapath/udatapath_ofdatapath-crc32.o udatapath/udatapath_ofdatapath-datapath.o udatapath/udatapath_ofdatapath-dp_act.o udatapath/udatapath_ofdatapath-of_ext_msg.o udatapath/udatapath_ofdatapath-udatapath.o udatapath/udatapath_ofdatapath-private-msg.o udatapath/udatapath_ofdatapath-switch-flow.o udatapath/udatapath_ofdatapath-table-hash.o udatapath/udatapath_ofdatapath-table-linear.o ${LIB_OBJ}
	${LD}  -o $@ $^

tests/test-flows: tests/test-flows.o  ${LIB_OBJ}
	${LD}  -o $@ $^

tests/test-hmap: tests/test-hmap.o  ${LIB_OBJ}
	${LD}  -o $@ $^

tests/test-list: tests/test-list.o  ${LIB_OBJ}
	${LD}  -o $@ $^

tests/test-dhcp-client: tests/test-dhcp-client.o ${LIB_OBJ} 
	${LD}  -o $@ $^

tests/test-stp: tests/test-stp.o ${LIB_OBJ}
	${LD}  -o $@ $^

instrumentation/test-client.o: instrumentation/test-client.c
	${CC} ${CFLAGS} -I lib -I include -I udatapath -c -o $@ $^

instrumentation/test-client: instrumentation/test-client.o ${LIB_OBJ}
	${LD}  -o $@ $^

instrumentation/test-server.o: instrumentation/test-server.c
	${CC} ${CFLAGS} -I lib -I include -c -o $@ $^

instrumentation/test-server: instrumentation/test-server.o ${LIB_OBJ}
	${LD}  -o $@ $^


