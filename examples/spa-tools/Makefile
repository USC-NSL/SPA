TARGETS = msg-compare msg-compare.test msg-compare.bc
LLVM_CC = clang++ -emit-llvm
CC = g++
CXX_FLAGS = -Wall -Werror -g -I../../include
LIB_SPA_TEST_MODULE = ../../Release+Asserts/lib/libSpaTestModule.so


default: all

all: msg-compare msg-compare.test msg-compare.bc $(TARGETS)

%: %.cpp
	$(CC) $(CXX_FLAGS) -o $@ $<

%.bc: %.cpp
	$(LLVM_CC) $(CXX_FLAGS) -DENABLE_SPA -DENABLE_KLEE -c -o $@ $<

%.test: %.cpp $(LIB_SPA_TEST_MODULE)
	$(CC) $(CXX_FLAGS) --coverage -DENABLE_SPA -o $@ $< $(LIB_SPA_TEST_MODULE)

clean:
	rm -rf $(TARGETS)
	rm -rf *.gcda *.gcno
