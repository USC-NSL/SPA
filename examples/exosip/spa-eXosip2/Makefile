PATHS = client.paths server.paths
TARGETS = client server
LIBS = ../libeXosip2-3.6.0/libexosip2.o
BC_TARGETS = client.bc server.bc
BC_LIBS = ../libeXosip2-3.6.0/libexosip2.bc
CC = gcc -Wall -g -I/usr/local/bin/spa/examples/exosip/libosip2-3.6.0/include -I/usr/local/bin/spa/examples/exosip/libeXosip2-3.6.0/include
LLVM_CC = llvm-gcc -Wall -g -O0 --emit-llvm -I/usr/local/bin/spa/examples/exosip/libosip2-3.6.0/include -I/usr/local/bin/spa/examples/exosip/libeXosip2-3.6.0/include
LLVM_LD = llvm-ld -r -disable-opt
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spa
SPA = time spa --stand-alone --libc=uclibc --posix-runtime

default: all

all: $(TARGETS) $(BC_TARGETS)

run-spa: $(PATHS)

%: %.c
	$(CC) $^ $(LIBS) -o $@

%.o: %.c
	$(LLVM_CC) -DENABLE_SPA -c $^ -o $@

%.bc: %.o
	$(LLVM_LD) $^ $(BC_LIBS) -o $@

.PRECIOUS: $(PATHS)

client.paths: client.bc $(SPA_EXE)
	$(SPA) --path-file $@ --client $< 2>&1 | tee $@.log

server.paths: server.bc $(SPA_EXE)
	$(SPA) --path-file $@ --server $< 2>&1 | tee $@.log

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(PATHS)
	rm -f client.o
	rm -rf *.log klee-out-* klee-last
