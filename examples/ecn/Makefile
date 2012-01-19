TARGETS = ecn.bc ecn
OBJECTS = ecn.bc
CC = llvm-gcc -Wall -g --emit-llvm
#MAX = max --stand-alone --libc=uclibc --posix-runtime -write-cov -write-cvcs -write-paths -write-pcs -write-test-info
MAX = max --stand-alone -write-cov -write-cvcs -write-paths -write-pcs -write-test-info

default: $(TARGETS)

ecn: ecn.c
	gcc -Wall -g -o $@ $^

%.bc: %.c
	$(CC) -DENABLE_MAX -c -o $@ $^

run-max: ecn.bc
	$(MAX) $^ max
 
clean:
	rm -f $(TARGETS) $(OBJECTS)
