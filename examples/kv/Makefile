TARGETS = kv-local kv-client kv-server
CFLAGS = -Wall -g -std=c99 -I../../include
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

kv-local: kv-client-common.o
kv-client: kv-client-common.o
kv-server: kv-local.o
kv-local.bc: kv-client-common.bc.o
kv-client.bc: kv-client-common.bc.o
kv-server.bc: kv-local.bc.o

clean:
	rm -f $(TARGETS) $(TARGETS:=.bc) *.o
