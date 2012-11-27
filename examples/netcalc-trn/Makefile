BADINPUTS = BadInputs.untested BadInputs.tested
PATHS = netcalc-client.bc.client.paths netcalc-server.bc.server.paths
CLIENT_STATE = netcalc-client.state
CFGS = netcalc-client.pdf netcalc-server.pdf
TARGETS = netcalc-client.exe netcalc-server.exe
BC_TARGETS = netcalc-client.bc netcalc-server.bc
TEST_TARGETS = netcalc-client.e2etest.exe netcalc-server.e2etest.exe netcalc-client.htest.exe netcalc-server.htest.exe
LOG_TARGETS = netcalc-client.log.exe netcalc-server.log.exe
HEADERS = netcalc.h
LLVM_CC = /usr/local/bin/llvm-gcc4.2-2.9-x86_64-linux/bin/llvm-g++ -Wall -g --emit-llvm
CC = g++ -Wall -g
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spa
SPA = $(SPA_EXE) --stand-alone --libc=uclibc --posix-runtime
LIB_SPA_TEST_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaTestModule.so
LIB_SPA_LOG_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaLogModule.so


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(LOG_TARGETS)

%.exe: %.c $(HEADERS)
	$(CC) -o $@ $<

%.bc: %.c $(HEADERS)
	$(LLVM_CC) -DENABLE_SPA -c -o $@ $<

%.e2etest.exe: %.c $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

netcalc-client.htest.exe: netcalc-client.bc
	spaHandlerTest --client $< --output netcalc-client.htest.bc
	llc netcalc-client.htest.bc -o netcalc-client.htest.s
	$(CC) -o $@ netcalc-client.htest.s $(LIB_SPA_TEST_MODULE)

netcalc-server.htest.exe: netcalc-server.bc
	spaHandlerTest --server $< --output netcalc-server.htest.bc
	llc netcalc-server.htest.bc -o netcalc-server.htest.s
	$(CC) -o $@ netcalc-server.htest.s $(LIB_SPA_TEST_MODULE)
	rm netcalc-server.htest.s

%.log.exe: %.c $(HEADERS) $(LIB_SPA_LOG_MODULE)
	$(CC) -DENABLE_SPA -o $@ $< $(LIB_SPA_LOG_MODULE)

%.bc.client.paths: %.bc $(CLIENT_STATE) $(SPA_EXE)
	$(SPA) --init-values $(CLIENT_STATE) --client $< 2>&1 | tee $@.log

%.bc.server.paths: %.bc $(SPA_EXE)
	$(SPA) --server $< 2>&1 | tee $@.log

BadInputs.untested: $(PATHS)
	spaBadInputs -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log

BadInputs.tested: BadInputs.untested $(TEST_TARGETS)
	spaHandlerTest --client ./netcalc-client.htest.exe --server ./netcalc-server.htest.exe --input $< --output $@ 2>&1 | tee $@.log

netcalc-client.dot: netcalc-client.bc $(SPA_EXE)
	spa --stand-alone --dump-cfg $@ --client $^

netcalc-server.dot: netcalc-server.bc $(SPA_EXE)
	spa --stand-alone --dump-cfg $@ --server $^

%.pdf: %.dot
	dot -Tpdf -o $@ $<

cfg: $(CFGS)

run-spa: BadInputs.tested

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(LOG_TARGETS) $(PATHS) $(BADINPUTS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -rf *.log klee-out-* klee-last
