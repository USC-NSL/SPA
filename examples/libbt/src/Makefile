# -*- makefile -*-

srcdir := .
top_srcdir := ..


CC:=gcc
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

CPPFLAGS:=
CFLAGS:=-g -Wall -I../include -I$(top_srcdir)/include

LIBS:=-lresolv -luuid -lssl -lcurl  `curl-config --libs` -lm
LDFLAGS:=-g -L. 
VERSION=1.06
MAKEDEPEND=makedepend

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
# DO NOT DELETE

benc.o: ../include/config.h ../include/benc.h ../include/types.h
benc.o: ../include/util.h ../include/bts.h ../include/bterror.h
bitset.o: ../include/config.h ../include/bitset.h ../include/random.h
bitset.o: ../include/util.h
block.o: ../include/config.h
btcheck.o: ../include/config.h ../include/types.h ../include/util.h
btcheck.o: ../include/benc.h ../include/bts.h ../include/segmenter.h
btcheck.o: ../include/bitset.h ../include/context.h ../include/peer.h
btcheck.o: ../include/stream.h ../include/strbuf.h
bterror.o: ../include/bterror.h
btget.o: ../include/config.h ../include/util.h ../include/bts.h
btget.o: ../include/benc.h ../include/types.h ../include/random.h
btget.o: ../include/peer.h ../include/stream.h ../include/strbuf.h
btget.o: ../include/bitset.h ../include/segmenter.h ../include/context.h
btget.o: ../include/bterror.h ../include/udpproto.h ../include/peerexchange.h
btlist.o: ../include/config.h ../include/bts.h ../include/types.h
btlist.o: ../include/util.h ../include/benc.h
bts.o: ../include/config.h ../include/bts.h ../include/util.h
context.o: ../include/config.h ../include/random.h ../include/udpproto.h
context.o: ../include/util.h ../include/context.h ../include/types.h
context.o: ../include/segmenter.h ../include/bitset.h ../include/peer.h
context.o: ../include/stream.h ../include/strbuf.h ../include/bts.h
context.o: ../include/benc.h
peer.o: ../include/config.h ../include/bterror.h ../include/btmessage.h
peer.o: ../include/peer.h ../include/stream.h ../include/strbuf.h
peer.o: ../include/bitset.h ../include/segmenter.h ../include/util.h
peer.o: ../include/context.h ../include/types.h ../include/bts.h
peer.o: ../include/benc.h ../include/peerexchange.h
peerexchange.o: ../include/config.h ../include/peerexchange.h
peerexchange.o: ../include/types.h ../include/util.h ../include/context.h
peerexchange.o: ../include/segmenter.h ../include/bitset.h ../include/peer.h
peerexchange.o: ../include/stream.h ../include/strbuf.h ../include/bts.h
peerexchange.o: ../include/benc.h
random.o: ../include/config.h
segmenter.o: ../include/config.h ../include/util.h ../include/types.h
segmenter.o: ../include/segmenter.h ../include/bitset.h
strbuf.o: ../include/config.h ../include/util.h ../include/strbuf.h
stream.o: ../include/config.h ../include/bterror.h ../include/stream.h
stream.o: ../include/strbuf.h ../include/util.h
types.o: ../include/config.h ../include/util.h ../include/types.h
udpproto.o: ../include/udpproto.h ../include/util.h ../include/context.h
udpproto.o: ../include/types.h ../include/segmenter.h ../include/bitset.h
udpproto.o: ../include/peer.h ../include/stream.h ../include/strbuf.h
udpproto.o: ../include/random.h
util.o: ../include/config.h ../include/bterror.h ../include/util.h
