BADINPUTS = BadInputs.untested BadInputs.tested
PATHS = netcalc-client.bc.client.paths netcalc-server.bc.server.paths
CFGS = netcalc-client.pdf netcalc-server.pdf
TARGETS = netcalc-client netcalc-server
BC_TARGETS = netcalc-client.bc netcalc-server.bc
TEST_TARGETS = netcalc-client.test netcalc-server.test
HEADERS = netcalc.h
LLVM_CC = clang++ -emit-llvm
CC = g++
CXX_FLAGS = -Wall -Werror -g -I../../include
SPA = ../../Release+Asserts/bin/spa
SPABADINPUTS = ../../Release+Asserts/bin/spaBadInputs
SPAE2ETEST = ../../Release+Asserts/bin/spaE2ETest
LIB_SPA_TEST_MODULE = ../../Release+Asserts/lib/libSpaTestModule.so


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS)

%: %.cpp $(HEADERS)
	$(CC) $(CXX_FLAGS) -o $@ $<

%.bc: %.cpp $(HEADERS)
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.test: %.cpp $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

%.bc.client.paths: %.bc $(SPA)
	$(SPA) --client $< 2>&1 | tee $@.log

%.bc.server.paths: %.bc $(SPA)
	$(SPA) --server $< 2>&1 | tee $@.log

BadInputs.untested: $(PATHS)
	$(SPABADINPUTS) -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log

BadInputs.tested: BadInputs.untested $(TEST_TARGETS)
	$(SPAE2ETEST) $< $@ './netcalc-client.test 1 1 +' './netcalc-server.test' 2>&1 | tee $@.log

netcalc-client.dot: netcalc-client.bc $(SPA)
	$(SPA) --stand-alone --dump-cfg $@ --client $^

netcalc-server.dot: netcalc-server.bc $(SPA)
	$(SPA) --stand-alone --dump-cfg $@ --server $^

%.pdf: %.dot
	dot -Tpdf -o $@ $<

cfg: $(CFGS)

run-spa: BadInputs.tested

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(PATHS) $(BADINPUTS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -rf *.log *.debug klee-out-* klee-last
