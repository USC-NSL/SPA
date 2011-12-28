TARGETS = client.bc server.bc
# CC = gcc -Wall -g -c
CC = llvm-gcc -Wall -g --emit-llvm -c

default: $(TARGETS)

%.bc: %.c
	$(CC) -o $@ $<

clean:
	rm -f $(TARGETS)
