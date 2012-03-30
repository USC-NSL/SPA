PATHS = netcalc-client.bc.paths netcalc-server.bc.paths
TARGETS = netcalc-client netcalc-server
BC_TARGETS = netcalc-client.bc netcalc-server.bc
HEADERS = netcalc.h
LLVM_CC = /usr/local/bin/llvm-gcc4.2-2.9-x86_64-linux/bin/llvm-g++ -Wall -g --emit-llvm
CC = g++ -Wall -g
SPA = spa --stand-alone --libc=uclibc --posix-runtime

default: all

all: $(TARGETS) $(BC_TARGETS)

%: %.c $(HEADERS)
	$(CC) -o $@ $<

%.bc: %.c $(HEADERS)
	$(LLVM_CC) -DENABLE_SPA -c -o $@ $<

%.bc.paths: %.bc
	$(SPA) $<

run-spa: $(PATHS)
	spaBadInputs -client netcalc-client.bc.paths -server netcalc-server.bc.paths

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(PATHS)
	rm -rf klee-out-* klee-last
