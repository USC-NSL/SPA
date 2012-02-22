TARGETS = ecn.bc ecn-max ecn
OBJECTS = ecn.bc
LLVM_CC = llvm-gcc -Wall -g --emit-llvm
CC = gcc -Wall -g
LIB_MAX_ADVERSARIAL_MODULE = ../../Release+Asserts/lib/libMaxAdversarialModule.so
MAX = max --stand-alone --libc=uclibc --posix-runtime -no-output
# MAX = max --stand-alone -no-output

default: all

all: $(TARGETS)

ecn-max: ecn.c $(LIB_MAX_ADVERSARIAL_MODULE)
	$(CC) -DENABLE_MAX -o $@ $^

ecn: ecn.c
	$(CC) -o $@ $^

%.bc: %.c
	$(LLVM_CC) -DENABLE_MAX -c -o $@ $^

run-max: ecn.bc
	$(MAX) $^
 
clean:
	rm -f $(TARGETS) $(OBJECTS)
