#!/bin/bash
make clean
./configure CPPFLAGS="-DENABLE_SPA -I`pwd`/../../include"
cd src
echo "
bttest: \${LIBBT} btget.c
	\${CC} \${CFLAGS} -I/home/kasey/spa/include -DVERSION=\${VERSION} \${LDFLAGS} --coverage -DENABLE_SPA -o \$@ btget.c -lbt \${LIBS} /usr/local/lib/libSpaTestModule.so" >> Makefile

make bttest

mv bttest ..

