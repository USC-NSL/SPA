BADINPUTS = BadInputs.untested BadInputs.tested
PATHS = netcalc-client.bc.client.paths netcalc-server.bc.server.paths
CFGS = netcalc-client.pdf netcalc-server.pdf
TARGETS = netcalc-client netcalc-server
BC_TARGETS = netcalc-client.bc netcalc-server.bc
TEST_TARGETS = netcalc-client.test netcalc-server.test
LOG_TARGETS = netcalc-client.logtest netcalc-server.logtest
HEADERS = netcalc.h
LLVM_CC = /usr/local/bin/llvm-gcc4.2-2.9-x86_64-linux/bin/llvm-g++ -Wall -g --emit-llvm
CC = g++ -Wall -g
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spa
SPA = $(SPA_EXE) --stand-alone --libc=uclibc --posix-runtime
LIB_SPA_TEST_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaTestModule.so
LIB_SPA_LOG_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaLogModule.so


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(LOG_TARGETS)

%: %.c $(HEADERS)
	$(CC) -o $@ $<

%.bc: %.c $(HEADERS)
	$(LLVM_CC) -DENABLE_SPA -c -o $@ $<

%.test: %.c $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

%.logtest: %.c $(HEADERS) $(LIB_SPA_LOG_MODULE)
	$(CC) -DENABLE_SPA -o $@ $< $(LIB_SPA_LOG_MODULE)

%.bc.client.paths: %.bc $(SPA_EXE)
	$(SPA) --client $^ 2>&1 | tee $@.log

%.bc.server.paths: %.bc $(SPA_EXE)
	$(SPA) --server $^ 2>&1 | tee $@.log

BadInputs.untested: $(PATHS)
	spaBadInputs -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log

BadInputs.tested: BadInputs.untested $(TEST_TARGETS)
	spaTest $< $@ './netcalc-client.test 1 1 +' './netcalc-server.test' 2>&1 | tee $@.log

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
