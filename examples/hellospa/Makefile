TARGETS = hellospa.exe
BC_TARGETS = hellospa.bc
INPUTS = hellospa.in
PATHS = hellospa.paths
CFGS = hellospa.pdf
LLVM_CC = llvm-gcc -Wall -O0 -g --emit-llvm
CC = gcc -Wall -g
SPA_EXE = /usr/local/bin/spa/Release+Asserts/bin/spa
SPA = $(SPA_EXE) --stand-alone --libc=uclibc --posix-runtime
LIB_SPA_TEST_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaTestModule.so
LIB_SPA_LOG_MODULE = /usr/local/bin/spa/Release+Asserts/lib/libSpaLogModule.so


default: all

all: $(TARGETS) $(BC_TARGETS)

%.exe: %.c
	$(CC) -o $@ $<

%.bc: %.c
	$(LLVM_CC) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.paths: %.bc $(INPUTS) $(SPA_EXE)
	$(SPA) --path-file $@ --client $< 2>&1 | tee $@.log
# 	$(SPA) --init-values $(INPUTS) --path-file $@ --client $< 2>&1 | tee $@.log

%.dot: %.bc $(SPA_EXE)
	spa --stand-alone --dump-cfg $@ --path-file /dev/null --client $<

%.pdf: %.dot
	dot -Tpdf -o $@ $<

run-spa: $(PATHS) $(CFGS)

clean:
	rm -f $(TARGETS) $(BC_TARGETS) $(PATHS) $(CFGS) $(CFGS:.pdf=.dot)
	rm -rf *.log klee-out-* klee-last
