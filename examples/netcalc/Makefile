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

run-spa: $(BC_TARGETS)
	$(SPA) $^

clean:
	rm -f $(TARGETS) $(BC_TARGETS)
