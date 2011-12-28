TARGETS = ecn_max.bc ecn_max ecn
OBJECTS = ecn.bc max.bc
CC = llvm-gcc -Wall -g --emit-llvm
LD = llvm-ld
KLEE = klee --libc=uclibc --posix-runtime -write-cov -write-cvcs -write-paths -write-pcs -write-test-info
#KLEE = klee -write-cov -write-cvcs -write-paths -write-pcs -write-test-info

default: $(TARGETS)

ecn: ecn.c
	gcc -Wall -g -o $@ $^

ecn_max.bc ecn_max: $(OBJECTS)
	$(LD) -o $(@:.bc=) $^

%.bc: %.c
	$(CC) -DENABLE_MAX -c -o $@ $^

run-klee: ecn_max.bc
	$(KLEE) $^ max
 
clean:
	rm -f $(TARGETS) $(OBJECTS)
