CFLAGS = -g -O0 -Wall -Werror -Wextra -Wno-missing-field-initializers         \
         -Wno-unused-parameter -I.
LDFLAGS = -g
CPPFLAGS =
DEPFLAGS = -MMD -MF $@.d
MODS := allocator preload library debug maps x86_decode securemem sandbox     \
        syscall_entrypoint system_call_table                                  \
        trusted_thread trusted_thread_asm trusted_process                     \
        access exit fault_handler_asm clone                                   \
        getpid gettid ioctl ipc madvise mmap mprotect                         \
        munmap open prctl reference_trusted_thread sigaction sigprocmask      \
        socketcall stat tls_setup tls_setup_helper
TEST_MODS := \
        tests/clone_test_helper \
        tests/syscall_via_int0 \
        tests/test_runner \
        tests/test_patching \
        tests/test_patching_input \
        tests/test_syscalls
OBJS64 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o64/')
OBJS32 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o32/')
TEST_OBJS64 := $(shell echo ${TEST_MODS} | xargs -n 1 | sed -e 's/$$/.o64/')
TEST_OBJS32 := $(shell echo ${TEST_MODS} | xargs -n 1 | sed -e 's/$$/.o32/')
ALL_OBJS = $(OBJS32) $(OBJS64) \
           $(TEST_OBJS32) $(TEST_OBJS64) \
           timestats.o playground.o
DEP_FILES = $(wildcard $(foreach f,$(ALL_OBJS),$(f).d))

include $(DEP_FILES)

.SUFFIXES: .o64 .o32

all: testbin timestats demo elf_loader_32 elf_loader_64 patch_offline

clean:
	-rm -f playground playground.o
	-rm -f $(ALL_OBJS)
	-rm -f $(DEP_FILES)
	-rm -f preload64.so
	-rm -f preload32.so
	-rm -f testbin64 testbin.o64
	-rm -f testbin32 testbin.o32
	-rm -f timestats timestats.o
	-rm -f run_tests_32 run_tests_64
	-rm -f elf_loader_32 elf_loader_64
	-rm -f patch_offline
	-rm -f core core.* vgcore vgcore.* strace.log*

test: run_tests_64 run_tests_32
	./run_tests_64
	./run_tests_32
	env SECCOMP_SANDBOX_REFERENCE_IMPL=1 ./run_tests_64
	env SECCOMP_SANDBOX_REFERENCE_IMPL=1 ./run_tests_32

run_tests_64: $(OBJS64) $(TEST_OBJS64)
	g++ -m64 $^ -lpthread -lutil -o $@
	@paxctl -cPSmXER "$@" 2>/dev/null || :
run_tests_32: $(OBJS32) $(TEST_OBJS32)
	g++ -m32 $^ -lpthread -lutil -o $@
	@paxctl -cPSmXER "$@" 2>/dev/null || :

# Link these as PIEs so that they stay out of the way of any
# fixed-position executable that gets loaded later.
elf_loader_64: elf_loader.o64 $(OBJS64)
	g++ -pie -m64 $^ -o $@
elf_loader_32: elf_loader.o32 $(OBJS32)
	g++ -pie -m32 $^ -o $@

patch_offline: patch_offline.o x86_decode.o
	g++ $^ -o $@

demo: playground preload32.so preload64.so
	./playground /bin/ls $(HOME)

testbin: testbin32 testbin64

gdb: testbin64
	gdb $<

valgrind: testbin64
	valgrind --db-attach=yes ./$<

strace: testbin32
	@rm -f strace.log*
	strace -ff -o strace.log ./$< &
	@/bin/bash -c 'sleep 0.25; sed -e "/In secure mode/q;d" <(tail -f $$(ls strace.log*|head -n 1))'
	multitail -mb 1GB -CS strace strace.log*

timestats: timestats.o
	${CXX} ${LDFLAGS} -o $@ $<

testbin64: test.cc ${OBJS64}
	${CXX} ${CFLAGS} ${CPPFLAGS} -m64 -c -o testbin.o64 $<
	${CXX} ${LDFLAGS} -m64 -o testbin64 testbin.o64 ${OBJS64} -lpthread -ldl
	@paxctl -cPSmXER "$@" 2>/dev/null || :

testbin32: test.cc ${OBJS32}
	${CXX} ${CFLAGS} ${CPPFLAGS} -m32 -c -o testbin.o32 $<
	${CXX} ${LDFLAGS} -m32 -o testbin32 testbin.o32 ${OBJS32} -lpthread -ldl
	@paxctl -cPSmXER "$@" 2>/dev/null || :

playground: playground.o
	${CXX} ${LDFLAGS} -o $@ $<

.cc.o:
	${CXX} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -c -o $@ $<

preload64.so: ${OBJS64}
	${CXX} ${LDFLAGS} -m64 -shared -o $@ $+ -lpthread

preload32.so: ${OBJS32}
	${CXX} ${LDFLAGS} -m32 -shared -o $@ $+ -lpthread

.cc.o64:
	${CXX} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m64 -fPIC -c -o $@ $<

.c.o64:
	${CC} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m64 --std=gnu99 -fPIC \
		-c -o $@ $<

.S.o64:
	${CC} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m64 -c -o $@ $<

.cc.o32:
	${CXX} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m32 -fPIC -c -o $@ $<

.c.o32:
	${CC} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m32 --std=gnu99 -fPIC \
		-c -o $@ $<

.S.o32:
	${CC} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -m32 -c -o $@ $<
