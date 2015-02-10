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
LIB_SPA_TEST_MODULE = $(shell dirname $(shell dirname $(shell which spa-pic)))/lib/libSpaTestModule.so


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS)

%: %.cpp $(HEADERS)
	$(CC) $(CXX_FLAGS) -o $@ $<

%.bc: %.cpp $(HEADERS)
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.test: %.cpp $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) --coverage -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

netcalc-client.dot: netcalc-client.bc $(shell which spa-pic)
	spa-pic --dump-cfg $@ --client $<

netcalc-server.dot: netcalc-server.bc $(shell which spa-pic)
	spa-pic --dump-cfg $@ --server $<

%.pdf: %.dot
	dot -Tpdf -o $@ $<

%.png: %.dot
	dot -Tpng -o $@ $<

cfg: $(CFGS)

# netcalc-client.paths: netcalc-client.bc $(shell which spa-pic)
# 	rm -f $@
# 	spa-pic --client $< --path-file $@ 2>&1 | tee $@.log
netcalc-client.paths: netcalc-client.bc $(shell which spa-explore)
	spa-explore \
		--out-paths $@ \
		--start-from SpaExecuteQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

# netcalc-server.paths: netcalc-server.bc $(shell which spa-pic)
# 	rm -f $@
# 	spa-pic --server $< 2>&1 | tee $@.log
# netcalc-server.paths: netcalc-server.bc netcalc-client.paths $(shell which spa-pic)
# 	rm -f $@
# 	spa-pic --sender-paths netcalc-client.paths --server $< --path-file $@ 2>&1 | tee $@.log
netcalc-server.paths: netcalc-server.bc netcalc-client.paths
	spa-explore \
		--in-paths netcalc-client.paths --connect-default \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--away-from spa_valid_path_point \
		--output-terminal \
		--output-at "NOT REACHING spa_valid_path_point" \
		--filter-output "REACHED spa_msg_input_point \
		                 AND NOT REACHED spa_valid_path_point" \
		$< 2>&1 | tee $@.log

# BadInputs.untested: $(PATHS) $(shell which spaBadInputs)
# 	spaBadInputs -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log
# BadInputs.untested: netcalc-server.bc.server.paths $(shell which spaBadInputs)
# 	rm -f $@
# 	spaBadInputs -server netcalc-server.bc.server.paths -o $@ -d $@.debug 2>&1 | tee $@.log

# validated.paths: netcalc-server.paths $(TEST_TARGETS) $(shell which spa-validate)
# 	rm -f $@
# 	killall netcalc-client.test netcalc-server.test || true
# 	spaE2ETest $< $@ false-positives.paths 3141 './netcalc-client.test 1 1 +' './netcalc-server.test' 2>&1 | tee $@.log
validated.paths: netcalc-server.paths $(TEST_TARGETS) $(shell which spa-validate)
	killall netcalc-client.test netcalc-server.test || true
	spa-validate $< $@ false-positives.paths \
		"RUN ./netcalc-server.test; \
		WAIT LISTEN UDP 3141; \
		RUN ./netcalc-client.test 0 0 +; \
		WAIT DONE 2; \
		KILL 1; \
		TIMEOUT 2000; \
		CHECK REACHED spa_msg_input_point; \
		CHECK NOT REACHED spa_valid_path_point;" \
		2>&1 | tee $@.log

cluster: validated.paths
	spa-cluster-manual $<

parallel-cluster:
	parallel-pic.sh $(BC_TARGETS) \
		"spa/examples/netcalc-msg" \
		"./netcalc-client.test 0 0 +" "./netcalc-server.test" 3141 \
		netcalc-client.paths netcalc-server.paths validated.paths

.PRECIOUS: $(PATHS) $(BADINPUTS)

clean:
	rm -rf $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) $(PATHS) $(BADINPUTS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -f netcalc-client.paths netcalc-server.paths
	rm -f validated.paths false-positives.paths
	rm -f default.paths netcalcDiv0.paths netcalcImplicitArg.paths
	rm -rf *.gcda *.gcno *.log *.debug klee-out-* klee-last
