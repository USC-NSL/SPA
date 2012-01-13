TARGETS = ecn.bc ecn
OBJECTS = ecn.bc
CC = llvm-gcc -Wall -g --emit-llvm
KLEE = klee --libc=uclibc --posix-runtime -write-cov -write-cvcs -write-paths -write-pcs -write-test-info
#KLEE = klee -write-cov -write-cvcs -write-paths -write-pcs -write-test-info

default: $(TARGETS)

ecn: ecn.c
	gcc -Wall -g -o $@ $^

%.bc: %.c
	$(CC) -DENABLE_MAX -c -o $@ $^

run-klee: ecn.bc
	$(KLEE) $^ max
 
clean:
	rm -f $(TARGETS) $(OBJECTS)
