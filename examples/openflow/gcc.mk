CFLAGS=-O0 -g -DENABLE_INSTRUMENTATION
CXXFLAGS=-O0 -g

LIB_OBJ=lib/dhcp-client.o lib/util.o lib/vlog.o lib/vconn-unix.o lib/dynamic-string.o lib/fatal-signal.o lib/dirs.o lib/hash.o lib/backtrace.o lib/learning-switch.o lib/svec.o lib/bitmap.o lib/random.o lib/port-array.o lib/vconn.o lib/ofpbuf.o lib/vconn-stream.o lib/fault.o lib/vconn-netlink.o lib/stp.o lib/tag.o lib/process.o lib/flow.o lib/leak-checker.o lib/dhcp.o lib/socket-util.o lib/queue.o lib/hmap.o lib/vconn-tcp.o lib/poll-loop.o lib/timeval.o lib/ofp-print.o lib/list.o lib/daemon.o lib/pcap.o lib/dpif.o lib/signals.o lib/command-line.o lib/rconn.o lib/netdev.o lib/ofpstat.o lib/shash.o lib/mac-learning.o lib/vlog-socket.o lib/csum.o lib/netlink.o

BIN=instrumentation/test-server instrumentation/test-client
.PHONY: all clean

all: ${BIN}

clean:
	rm -f ${BIN} instrumentation/test-server.o instrumentation/test-client.o

instrumentation/test-server.o: instrumentation/test-server.c
	${CC} ${CFLAGS} -I lib -I include -c -o $@ $^

instrumentation/test-server: instrumentation/test-server.o ${LIB_OBJ}
	${CC} -o $@ $^ -ldl

instrumentation/test-client.o: instrumentation/test-client.c
	${CC} ${CFLAGS} -I udatapath -I lib -I include -c -o $@ $^

instrumentation/test-client: instrumentation/test-client.o ${LIB_OBJ} udatapath/udatapath_ofdatapath-datapath.o udatapath/udatapath_ofdatapath-chain.o udatapath/udatapath_ofdatapath-udatapath.o udatapath/udatapath_ofdatapath-switch-flow.o udatapath/udatapath_ofdatapath-dp_act.o udatapath/udatapath_ofdatapath-private-msg.o udatapath/udatapath_ofdatapath-of_ext_msg.o udatapath/udatapath_ofdatapath-table-hash.o udatapath/udatapath_ofdatapath-table-linear.o udatapath/udatapath_ofdatapath-crc32.o
	${CC} -o $@ $^ -ldl

