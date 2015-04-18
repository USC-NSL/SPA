BADINPUTS = BadInputs.untested BadInputs.tested BadInputs.FalsePositives
TARGETS = netcalc-client netcalc-server netcalc-proxy
BC_TARGETS = netcalc-client.bc netcalc-server.bc netcalc-proxy.bc
TEST_TARGETS = netcalc-client.test netcalc-server.test netcalc-proxy.test
FIXED_TARGETS = netcalc-client-fixed netcalc-server-fixed netcalc-proxy-fixed
FIXED_BC_TARGETS = netcalc-client-fixed.bc netcalc-server-fixed.bc netcalc-proxy-fixed.bc
FIXED_TEST_TARGETS = netcalc-client-fixed.test netcalc-server-fixed.test netcalc-proxy-fixed.test
CFGS = netcalc-client.cfg netcalc-server.cfg
HEADERS = netcalc.h
LLVM_CC = clang++ -emit-llvm
CC = g++
CXX_FLAGS = -Wall -Werror -g -I../../include
LIB_SPA_TEST_MODULE = ../../Release+Asserts/lib/libSpaTestModule.so
LIB_MSG_COMPARE_BC = ../spa-tools/msg-compare.bc


default: all

all: $(TARGETS) $(BC_TARGETS) $(TEST_TARGETS) \
     $(FIXED_TARGETS) $(FIXED_BC_TARGETS) $(FIXED_TEST_TARGETS)

%: %.cpp $(HEADERS)
	$(CC) $(CXX_FLAGS) -o $@ $<

%.bc: %.cpp $(HEADERS)
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.test: %.cpp $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) --coverage -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

%-fixed: %.cpp $(HEADERS)
	$(CC) $(CXX_FLAGS) -DENABLE_FIX -o $@ $<

%-fixed.bc: %.cpp $(HEADERS)
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -DENABLE_FIX -c -o $@ $<

%-fixed.test: %.cpp $(HEADERS) $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) --coverage -DENABLE_SPA -DENABLE_FIX -o $@ $< $(LIB_SPA_TEST_MODULE)

netcalc-client.cfg: netcalc-client.bc $(shell which spa-pic)
	spa-pic --dump-cfg $@ --client $<

netcalc-server.cfg: netcalc-server.bc $(shell which spa-pic)
	spa-pic --dump-cfg $@ --server $<

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

netcalc-client-fixed.paths: netcalc-client-fixed.bc $(shell which spa-explore)
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
		--in-paths netcalc-client.paths --connect-in-out \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--away-from spa_valid_path_point \
		--output-terminal \
		--output-at "NOT REACHING spa_valid_path_point" \
		--filter-output "REACHED spa_msg_input_point \
		                 AND NOT REACHED spa_valid_path_point" \
		$< 2>&1 | tee $@.log

# BadInputs.untested: netcalc-client.bc.client.paths netcalc-server.bc.server.paths $(shell which spaBadInputs)
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
	spa-validate -d $< $@ false-positives.paths \
		"RUN ./netcalc-server.test; \
		WAIT LISTEN UDP 3141; \
		RUN ./netcalc-client.test 0 0 +; \
		WAIT DONE 2; \
		KILL 1; \
		TIMEOUT 2000; \
		CHECK REACHED spa_msg_input_point; \
		CHECK NOT REACHED spa_valid_path_point;" \
		2>&1 | tee $@.log

netcalc-server-sequiv1.paths: netcalc-server.bc $(shell which spa-explore)
	spa-explore \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-server-sequiv2.paths: netcalc-server-fixed.bc netcalc-server-sequiv1.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-server-sequiv1.paths --connect-in-in \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-server-sequiv3.paths: $(LIB_MSG_COMPARE_BC) netcalc-server-sequiv2.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-server-sequiv2.paths \
		--connect spa_in_msg_message1=spa_out_msg_response \
		--connect spa_in_msg_size_message1=spa_out_msg_size_response \
		--connect spa_in_msg_message2=spa_out_msg_response2 \
		--connect spa_in_msg_size_message2=spa_out_msg_size_response2 \
		--out-paths $@ \
		--start-from MsgCompareEntry \
		--output-at MsgsDifferent \
		--max-instruction-time=5 \
		$< 2>&1 | tee $@.log

netcalc-server-csequiv1.paths: netcalc-server.bc netcalc-client-fixed.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-client-fixed.paths --connect-in-out \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-server-csequiv2.paths: netcalc-server-fixed.bc netcalc-server-csequiv1.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-server-csequiv1.paths --connect-in-in \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-server-csequiv3.paths: $(LIB_MSG_COMPARE_BC) netcalc-server-csequiv2.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-server-csequiv2.paths \
		--connect spa_in_msg_message1=spa_out_msg_response \
		--connect spa_in_msg_size_message1=spa_out_msg_size_response \
		--connect spa_in_msg_message2=spa_out_msg_response2 \
		--connect spa_in_msg_size_message2=spa_out_msg_size_response2 \
		--out-paths $@ \
		--start-from MsgCompareEntry \
		--output-at MsgsDifferent \
		--max-instruction-time=5 \
		$< 2>&1 | tee $@.log

netcalc-proxy-csequiv1.paths: netcalc-proxy.bc netcalc-client-fixed.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-client-fixed.paths \
		--connect spa_in_msg_in_query=spa_out_msg_query \
		--connect spa_in_msg_size_in_query=spa_out_msg_size_query \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-proxy-csequiv2.paths: netcalc-server.bc netcalc-proxy-csequiv1.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-proxy-csequiv1.paths \
		--connect spa_in_msg_query=spa_out_msg_out_query \
		--connect spa_in_msg_size_query=spa_out_msg_size_out_query \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-proxy-csequiv3.paths: netcalc-server.bc netcalc-proxy-csequiv2.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-proxy-csequiv2.paths --connect-in-out \
		--out-paths $@ \
		--start-from SpaHandleQueryEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		$< 2>&1 | tee $@.log

netcalc-proxy-csequiv4.paths: $(LIB_MSG_COMPARE_BC) netcalc-proxy-csequiv3.paths $(shell which spa-explore)
	spa-explore \
		--in-paths netcalc-proxy-csequiv3.paths \
		--connect spa_in_msg_message1=spa_out_msg_response \
		--connect spa_in_msg_size_message1=spa_out_msg_size_response \
		--connect spa_in_msg_message2=spa_out_msg_response2 \
		--connect spa_in_msg_size_message2=spa_out_msg_size_response2 \
		--out-paths $@ \
		--start-from MsgCompareEntry \
		--output-at MsgsDifferent \
		--max-instruction-time=5 \
		$< 2>&1 | tee $@.log

cluster: validated.paths
	spa-cluster-manual $<

parallel-cluster:
	parallel-pic.sh $(BC_TARGETS) \
		"spa/examples/netcalc-msg" \
		"./netcalc-client.test 0 0 +" "./netcalc-server.test" 3141 \
		netcalc-client.paths netcalc-server.paths validated.paths

interop: validated.paths
server-equiv: netcalc-server-sequiv3.paths
client-server-equiv: netcalc-server-csequiv3.paths
proxy-equiv: netcalc-proxy-csequiv4.paths

.PRECIOUS: *.paths $(BADINPUTS)

clean:
	rm -rf $(TARGETS) $(FIXED_TARGETS) $(BC_TARGETS) $(FIXED_BC_TARGETS) $(TEST_TARGETS) $(FIXED_TEST_TARGETS) $(BADINPUTS) $(CFGS)
	rm -rf *.gcda *.gcno *.paths *.log *.debug klee-out-* klee-last
