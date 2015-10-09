TARGETS = kv-local kv-client kv-server
CFLAGS = -Wall -g -std=c++11 -I../../include

.SUFFIXES:

default: $(TARGETS) $(TARGETS:=.bc)

%.o: %.cpp
	g++ $(CFLAGS) -c $< -o $@

%: %.o
	g++ -o $@ $^

%.bc.o: %.cpp
	clang++ $(CFLAGS) -DENABLE_SPA -DENABLE_KLEE -emit-llvm -c $< -o $@

%.bc: %.bc.o
	llvm-link -o $@ $^

kv-local: kv-client-common.o
kv-client: kv-client-common.o
kv-local.bc: kv-client-common.bc.o
kv-client.bc: kv-client-common.bc.o

clean:
	rm -f $(TARGETS) $(TARGETS:=.bc) *.o
