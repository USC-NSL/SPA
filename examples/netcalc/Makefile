BADINPUTS = BadInputs.txt
PATHS = netcalc-client.bc.client.paths netcalc-server.bc.server.paths
CFGS = netcalc-client.pdf netcalc-server.pdf
TARGETS = netcalc-client netcalc-server
BC_TARGETS = netcalc-client.bc netcalc-server.bc
HEADERS = netcalc.h
LLVM_CC = /usr/local/bin/llvm-gcc4.2-2.9-x86_64-linux/bin/llvm-g++ -Wall -g --emit-llvm
CC = g++ -Wall -g
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spa
SPA = $(SPA_EXE) --stand-alone --libc=uclibc --posix-runtime

default: all

all: $(TARGETS) $(BC_TARGETS)

%: %.c $(HEADERS)
	$(CC) -o $@ $<

%.bc: %.c $(HEADERS)
	$(LLVM_CC) -DENABLE_SPA -c -o $@ $<

%.bc.client.paths: %.bc $(SPA_EXE)
	$(SPA) --client $^ 2>&1 | tee $@.log

%.bc.server.paths: %.bc $(SPA_EXE)
	$(SPA) --server $^ 2>&1 | tee $@.log

$(BADINPUTS): $(PATHS)
	spaBadInputs -client netcalc-client.bc.client.paths -server netcalc-server.bc.server.paths 2>&1 > $(BADINPUTS) | tee $(BADINPUTS).log

netcalc-client.dot: netcalc-client.bc $(SPA_EXE)
	spa --stand-alone --dump-cfg $@ --client $^

netcalc-server.dot: netcalc-server.bc $(SPA_EXE)
	spa --stand-alone --dump-cfg $@ --server $^

%.pdf: %.dot
	dot -Tpdf -o $@ $<

cfg: $(CFGS)

run-spa: $(BADINPUTS)

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(PATHS) $(BADINPUTS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -rf *.log klee-out-* klee-last
