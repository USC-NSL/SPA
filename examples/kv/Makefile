TARGETS = kv-local kv-client-udp kv-server-udp kv-client-tcp kv-server-tcp
CFLAGS = -Wall -O0 -g -std=c99 -I../../include
SPA_HEADER = ../../include/spa/spaRuntime.h

.SUFFIXES:

default: $(TARGETS) $(TARGETS:=.bc)

%.o: %.c $(SPA_HEADER)
	gcc $(CFLAGS) -c $< -o $@

%: %.o
	gcc -o $@ $^

%.bc.o: %.c $(SPA_HEADER)
	clang $(CFLAGS) -DENABLE_SPA -DENABLE_KLEE -emit-llvm -c $< -o $@

%.bc: %.bc.o
	llvm-link -o $@ $^

kv-local kv-client-udp kv-client-tcp: kv-client-common.o
kv-server-udp kv-server-tcp: kv-local.o
kv-local.bc kv-client-udp.bc kv-client-tcp.bc: kv-client-common.bc.o
kv-server-udp.bc kv-server-tcp.bc: kv-local.bc.o

clean:
	rm -f $(TARGETS) $(TARGETS:=.bc) *.o
