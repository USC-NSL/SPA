TARGETS = ecn.bc ecn
OBJECTS = ecn.bc
CC = llvm-gcc -Wall -g --emit-llvm
MAX = max --stand-alone --libc=uclibc --posix-runtime -no-output
# MAX = max --stand-alone -no-output

default: all

all: $(TARGETS)

ecn: ecn.c
	gcc -Wall -g -o $@ $^

%.bc: %.c
	$(CC) -DENABLE_MAX -c -o $@ $^

run-max: ecn.bc
	$(MAX) $^
 
clean:
	rm -f $(TARGETS) $(OBJECTS)
