TARGETS = ecn.bc ecn-max ecn
OBJECTS = ecn.bc
LLVM_CC = clang -emit-llvm
CC = g++
CFLAGS = -Wall -Werror -g -I../../include
LIB_MAX_ADVERSARIAL_MODULE = ../../Release+Asserts/lib/libMaxAdversarialModule.so

default: all

all: $(TARGETS)

ecn-max: ecn.c $(LIB_MAX_ADVERSARIAL_MODULE)
	$(CC) $(CFLAGS) -DENABLE_MAX -o $@ $^ /usr/local/src/stp/build/lib/libstp.a

ecn: ecn.c
	$(CC) $(CFLAGS) -o $@ $^

%.bc: %.c
	$(LLVM_CC) $(CFLAGS) -DENABLE_KLEE -DENABLE_MAX -c -o $@ $^

# run-max: ecn.bc
# 	max $^
max_paths.txt: ecn.bc
	spa-explore \
		--out-paths $@ \
		--start-from MaxSenderHandlePacketEntry \
		--toward max_interesting \
		--output-at max_interesting \
		--stop-at max_interesting \
		$< 2>&1 | tee $@.log

run-max: max_paths.txt

clean:
	rm -f $(TARGETS) $(OBJECTS)
	rm -rf klee-out-* klee-last
	rm -f max_paths.txt
