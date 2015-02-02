# -*- makefile -*-

srcdir := .
top_srcdir := ..


CC:=clang -emit-llvm
AR:=ar
RANLIB:=ranlib

PROGRAMS:=btlist btget btcheck
LIBBT:=libbt.a
SRCS:=\
	benc.c \
	bts.c \
	types.c \
	random.c \
	strbuf.c \
	stream.c \
	peer.c \
	segmenter.c \
	util.c \
	bitset.c \
	context.c \
	bterror.c \
        udpproto.c \
	peerexchange.c
OBJS:= ${SRCS:.c=.o}

CPPFLAGS:=-DENABLE_SPA -DENABLE_KLEE -I/home/kasey/spa/examples/libbt/../../include
CFLAGS:=-g -Wall -I../include -I$(top_srcdir)/include

LIBS:=-lcurl  -lssl -lcrypto -lssl `curl-config --libs` -lm
LDFLAGS:=-g -L. 
VERSION=1.06
MAKEDEPEND=

all: ${LIBBT} ${PROGRAMS}

install: all
	mv ${PROGRAMS} libbt.a ../bin
static: 
	-rm ${PROGRAMS}
	make "LDFLAGS=${LDFLAGS} -static"
docs: 
	doxygen *.c
depend:
	${MAKEDEPEND} ${CFLAGS} - ${SRCS}
btcheck: libbt.a btcheck.c
	${CC} ${CFLAGS} -DVERSION=${VERSION} ${LDFLAGS} -o btcheck btcheck.c -lbt ${LIBS}

btlist: ${LIBBT} btlist.c
	${CC} ${CFLAGS} -DVERSION=${VERSION} ${LDFLAGS} -o btlist btlist.c -lbt ${LIBS}

btget: ${LIBBT} btget.c
	${CC} ${CFLAGS} -DVERSION=${VERSION} ${LDFLAGS} -o btget btget.c -lbt ${LIBS}

clean:
	rm -f ${OBJS} btcheck.o btlist.o btget.o

distclean: clean
	rm -f ${PROGRAMS} ${LIBBT}
	cp Makefile.default Makefile

libbt.a: ${OBJS}
	${AR} rc $@ ${OBJS}
	${RANLIB} $@

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $< -o $@
