PATHS = client.paths server.paths
# LOGS = client.log server.log
TARGETS = client.exe server.exe
DBG_LIBS = ../libeXosip2-3.6.0/libexosip2-dbg.o
TEST_LIBS = ../libeXosip2-3.6.0/libexosip2-test.o
BC_TARGETS = client.bc server.bc
BC_LIBS = ../libeXosip2-3.6.0/libexosip2.bc
# TEST_TARGETS = client.htest.exe server.htest.exe
# LOG_TARGETS = client.log.exe server.log.exe
CC = gcc -Wall -g -I/usr/local/bin/spa/examples/exosip/libosip2-3.6.0/include -I/usr/local/bin/spa/examples/exosip/libeXosip2-3.6.0/include
LLVM_CC = llvm-gcc -Wall -g -O0 --emit-llvm -I/usr/local/bin/spa/examples/exosip/libosip2-3.6.0/include -I/usr/local/bin/spa/examples/exosip/libeXosip2-3.6.0/include
LLVM_LD = llvm-ld -r -disable-opt
LIB_SPA_TEST_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaTestModule.so
LIB_SPA_LOG_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaLogModule.so
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spaRR
SPA = time $(SPA_EXE) --stand-alone --libc=uclibc --posix-runtime

default: all

all: $(TARGETS) $(BC_TARGETS) $(LOG_TARGETS) $(TEST_TARGETS)

run-spa: $(PATHS)

%.exe: %.c $(DBG_LIBS)
	$(CC) $^ -o $@

# %.log.exe: %.c $(TEST_LIBS) $(LIB_SPA_LOG_MODULE)
# 	$(CC) -DENABLE_SPA -o $@ $^
# 
# %.log: %.log.exe
# 	SPA_LOG_FILE=$@ ./$<
# 
# %.htest.bc: %.c
# 	$(LLVM_CC) -DENABLE_SPA -c $< -o $@.o
# 	$(LLVM_LD) $(BC_LIBS) $@.o -o $@
# 	rm $@.o
# 
# client.htest.exe: client.htest.bc
# 	spaHandlerTest --client $< --output $<
# 	llc $< -jit-emit-debug -o client.htest.s
# 	$(CC) -o $@ client.htest.s $(LIB_SPA_TEST_MODULE)
# 	strip -x $@
# 	rm client.htest.s
# 
# server.htest.exe: server.htest.bc
# 	spaHandlerTest --server $< --output $<
# 	llc $< -o server.htest.s
# 	$(CC) -o $@ server.htest.s $(LIB_SPA_TEST_MODULE)
# 	strip -x $@
# 	rm server.htest.s

%.bc.o: %.c
	$(LLVM_CC) -DENABLE_SPA -DENABLE_KLEE -c $< -o $@

%.bc: %.bc.o $(BC_LIBS)
	$(LLVM_LD) $(BC_LIBS) $< -o $@

.PRECIOUS: $(PATHS) $(LOGS)

client.paths: client.bc $(SPA_EXE)
	$(SPA) --path-file $@ --client $< 2>&1 | tee $@.log

server.paths: server.bc $(SPA_EXE)
	$(SPA) --path-file $@ --server $< 2>&1 | tee $@.log

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(BC_TARGETS:.bc=.bc.o) $(TEST_TARGETS) $(TEST_TARGETS:.exe=.bc) $(LOG_TARGETS) $(PATHS) $(LOGS)
	rm -rf *.log klee-out-* klee-last
