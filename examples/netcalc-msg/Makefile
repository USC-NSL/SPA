BADINPUTS = BadInputs.untested BadInputs.tested BadInputs.FalsePositives
PATHS = netcalc-client.bc.client.paths netcalc-server.bc.server.paths
CFGS = netcalc-client.pdf netcalc-server.pdf
TARGETS = netcalc-client netcalc-server
BC_TARGETS = netcalc-client.bc netcalc-server.bc
TEST_TARGETS = netcalc-client.test netcalc-server.test
HEADERS = netcalc.h
LLVM_CC = clang++ -emit-llvm
CC = g++
CXX_FLAGS = -Wall -Werror -g -I../../include
SPA = $(shell which spa-pic)
SPABADINPUTS = $(shell which spaBadInputs)
SPAE2ETEST = $(shell which spaE2ETest)
LIB_SPA_TEST_MODULE = $(shell dirname $(shell dirname $(SPA)))/lib/libSpaTestModule.so


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS)

%: %.cpp $(HEADERS)
	$(CC) $(CXX_FLAGS) -o $@ $<

%.bc: %.cpp $(HEADERS)
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.test: %.cpp $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

%.bc.client.paths: %.bc $(SPA)
	rm -f $@
	$(SPA) --client $< 2>&1 | tee $@.log

# %.bc.server.paths: %.bc $(SPA)
# 	rm -f $@
# 	$(SPA) --server $< 2>&1 | tee $@.log
%.bc.server.paths: %.bc netcalc-client.bc.client.paths $(SPA)
	rm -f $@
	$(SPA) --sender-paths netcalc-client.bc.client.paths --server $< 2>&1 | tee $@.log

# BadInputs.untested: $(PATHS)
# 	$(SPABADINPUTS) -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log
BadInputs.untested: netcalc-server.bc.server.paths
	rm -f $@
	$(SPABADINPUTS) -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log

BadInputs.tested: BadInputs.untested $(TEST_TARGETS)
	rm -f $@
	$(SPAE2ETEST) $< $@ BadInputs.FalsePositives 3141 './netcalc-client.test 1 1 +' './netcalc-server.test' 2>&1 | tee $@.log

netcalc-client.dot: netcalc-client.bc $(SPA)
	$(SPA) --dump-cfg $@ --client $<

netcalc-server.dot: netcalc-server.bc $(SPA)
	$(SPA) --dump-cfg $@ --server $<

%.pdf: %.dot
	dot -Tpdf -o $@ $<

%.png: %.dot
	dot -Tpng -o $@ $<

cfg: $(CFGS)

netcalc-client.paths: netcalc-client.bc
	spa-explore $< --out-paths $@ --start-from SpaExecuteQueryEntry --toward "AFTER spa_msg_output" --stop-at "AFTER spa_msg_output" --output-at "AFTER spa_msg_output"

netcalc-server.paths: netcalc-server.bc netcalc-client.paths
	spa-explore $< --in-paths netcalc-client.paths --out-paths $@ --start-from SpaHandleQueryEntry --away-from spa_valid_path --output-terminal --output-at "NOT REACHING spa_valid_path"

validated.paths: netcalc-server.path $(TEST_TARGETS)
	spa-validate -d $< $@ "RUN netcalc-server.exe; WAIT LISTEN TCP 8000; RUN netcalc-client.exe; TIMEOUT 2s; CHECK NOT REACHED spa_valid_path"

run-pic: BadInputs.tested

run-spa: netcalc-server.paths

.PRECIOUS: $(PATHS) $(BADINPUTS)

clean:
	rm -rf $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(PATHS) $(BADINPUTS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -rf *.log *.debug klee-out-* klee-last
