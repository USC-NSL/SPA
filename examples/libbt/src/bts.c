/* 
 * Copyright 2003,2004,2005 Kevin Smathers, All Rights Reserved
 *
 * This file may be used or modified without the need for a license.
 *
 * Redistribution of this file in either its original form, or in an
 * updated form may be done under the terms of the GNU LIBRARY GENERAL
 * PUBLIC LICENSE.  If this license is unacceptable to you then you
 * may not redistribute this work.
 * 
 * See the file COPYING.LGPL for details.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <stdarg.h>
#include <assert.h>
#include "bts.h"
#include "util.h"
#if WIN32
#	define vsnprintf _vsnprintf
#endif

/*
 * btStream subclasses are used to de/serialize the btObject hierarchy
 * from/into bencoded streams of bytes.
 */
typedef struct btFileStream {
#define BTFILESTREAM 1
    struct btStream bts;
    FILE *fp;
} btFileStream;

typedef struct btStrStream {
#define BTSTRSTREAM 2
    struct btStream bts;
    int cur;
    unsigned char *buf;
    int len;
    int max;
} btStrStream;

int bts_strstream_read(btStream*bts, char *buf, int len) {
    btStrStream *ss = (btStrStream *)bts;
    if (ss->bts.type != BTSTRSTREAM) return -1;
    if (ss->cur+len > ss->len) {
        return 1;
    }
    memcpy(buf, ss->buf + ss->cur, len);
    ss->cur += len;
    return 0;
}

int bts_strstream_peek(btStream*bts) {
    btStrStream *ss = (btStrStream *)bts;
    if (ss->bts.type != BTSTRSTREAM) return -1;
    if (ss->cur >= ss->len) {
        return -1;
    }
    return ss->buf[ss->cur];
}

int bts_strstream_write(btStream*bts, char *buf, int len) {
    btStrStream *ss = (btStrStream *)bts;
    if (ss->bts.type != BTSTRSTREAM) return -1;
    if (ss->len + len > ss->max) {
        /* resize buffer */
        ss->max = ss->len + len + 100;
        ss->buf = btrealloc(ss->buf, ss->max);
    }
    memcpy(ss->buf + ss->len, buf, len);
    ss->len += len;
    return 0;
}

void bts_strstream_destroy( btStream *bts) {
    btStrStream *ss = (btStrStream *)bts;
    if (!ss) return;
    if (ss->bts.type != BTSTRSTREAM) return;
    if (ss->buf) {
        btfree(ss->buf);
	ss->buf=NULL;
	ss->len=0;
	ss->max=0;
    }
    btfree( ss);
}

int bts_strstream_rewind( btStream *bts, btsIo iodir) {
    btStrStream *ss = (btStrStream *)bts;
    if (ss->bts.type != BTSTRSTREAM) return 1;
    ss->bts.iodir = iodir;
    if (iodir == BTS_INPUT) {
	ss->cur = 0;
    } else if (iodir == BTS_OUTPUT) {
	ss->len = 0;
    } else {
	return 1;
    }
    return 0;
}

btStream* bts_create_strstream( btsIo iodir) {
    btStrStream *bts = btcalloc( 1, sizeof(btStrStream));
    bts->bts.type = BTSTRSTREAM;
    bts->bts.iodir = iodir;
    bts->bts.read = bts_strstream_read;
    bts->bts.write = bts_strstream_write;
    bts->bts.peek = bts_strstream_peek;
    bts->bts.destroy = bts_strstream_destroy;
    bts->bts.rewind = bts_strstream_rewind;
    return &bts->bts;
}

int bts_filestream_read(btStream*bts, char *buf, int len) {
    btFileStream *fs = (btFileStream *)bts;
    int res;
    if (fs->bts.type != BTFILESTREAM) return -1;
    if (fs->fp == NULL) return -1;
    res = fread( buf, 1, len, fs->fp);
    if (res < len) return -1;
    return 0;
}

int bts_filestream_write(btStream*bts, char *buf, int len) {
    btFileStream *fs = (btFileStream *)bts;
    int res;
    if (fs->bts.type != BTFILESTREAM) return -1;
    if (fs->fp == NULL) return -1;
    res = fwrite( buf, 1, len, fs->fp);
    if (res < len) return -1;
    return 0;
}

size_t writebts( void *buf, size_t size, size_t nmemb, void* stream ) {
    btStream *bts=stream;
    int len=size*nmemb;
    if (bts_write(bts, buf, len) != 0) {
	return 0;
    }
    return len;
}

int bts_filestream_peek(btStream*bts) {
    btFileStream *fs = (btFileStream *)bts;
    int c;
    int err;
    if (fs->bts.type != BTFILESTREAM) return -1;
    if (fs->fp == NULL) return -1;
    c = getc( fs->fp);
    if (c == EOF) return -1;
    err = ungetc( c, fs->fp);
    if (err == EOF) return -1;
    return c;
}

void bts_filestream_destroy( btStream *bts) {
    btFileStream *fs = (btFileStream *)bts;
    if (!fs) return;
    if (fs->bts.type != BTFILESTREAM) return;
    if (fs->fp == NULL) return;
    fclose(fs->fp);
    fs->fp=NULL;
    btfree( fs);
}

int bts_filestream_rewind( btStream *bts, btsIo iodir) {
    btFileStream *fs = (btFileStream *)bts;
    if (fs->bts.type != BTFILESTREAM) return 1;
    printf("unimplemented bts_filestream_rewind()\n");
    return 1;
}

btStream* bts_create_filestream_fp( FILE *fp, btsIo iodir) {
    btFileStream *bts = btcalloc( 1, sizeof(btFileStream));
    bts->bts.type = BTFILESTREAM;

#if 0
    if (fp->flags & _IO_NO_READS) {
        iodir&=~BTS_OUTPUT;
    }
    if (fp->flags & _IO_NO_WRITES) { 
        iodir&=~BTS_INPUT;
    }
#endif 

    bts->bts.iodir = iodir;
    bts->bts.read = bts_filestream_read;
    bts->bts.write = bts_filestream_write;
    bts->bts.peek = bts_filestream_peek;
    bts->bts.destroy = bts_filestream_destroy;
    bts->bts.rewind = bts_filestream_rewind;
    bts->fp = fp;
    return &bts->bts;
}


btStream* bts_create_filestream( char *fname, btsIo iodir) {
    FILE *fp;
  
    if (iodir == BTS_INPUT) {
        fp = fopen( fname, "r");
    } else if (iodir == BTS_OUTPUT) {
        fp = fopen( fname, "w");
    } else {
        fp = NULL;
    }
    return bts_create_filestream_fp( fp, iodir);
}

void bts_destroy( btStream *bts) {
    bts->destroy(bts);
}

int bts_rewind( btStream* bts, btsIo iodir) {
    return bts->rewind(bts, iodir);
}


struct btstrbuf bts_get_buf( btStream *bts) {
    struct btstrbuf buf = { NULL, 0 };
    btStrStream *bss = (btStrStream*)bts;
    if (bts->type != BTSTRSTREAM) return buf;
    buf.buf = bss->buf;
    buf.len = bss->len;
    return buf;
}

int bts_read( btStream* bts, char *buf, int len) {
    if (len<0) return -1;
    if (len == 0) { 
    	*buf = 0; 
	return 0; 
    }
    return bts->read(bts,buf,len);
}

int bts_write( btStream* bts, char *buf, int len) {
    assert(len >= 0);
    return bts->write(bts,buf,len);
}

int bts_peek( btStream* bts) {
    return bts->peek(bts);
}

int bts_printf( btStream* bts, char *fmt, ...) {
    static char buf[8192];
    int res;
    int osize;
    va_list va;
    va_start(va, fmt);
    osize=vsnprintf(buf, sizeof(buf), fmt, va);
    DIE_UNLESS(osize < sizeof(buf));
    res = bts_write( bts, buf, osize);
    va_end(va);
    return res;
}

/* 
 * bts_scanbreak
 *   Read from an input stream and copy characters into 'buf' until hitting
 *   a break character.  If there are any characters that are not listed 
 *   in 'cset' then an error value is returned.  Scanbreak is used to read
 *   bEncoded integers.
 *
 *   bts   - I/O stream, must be an input stream
 *   cset  - List of allowed characters
 *   bset  - List of break characters
 *   buf   - Return value of data read from the stream
 *   bufsz - Available space in the buffer
 *
 * return 1 on error, 0 on success 
 */
int bts_scanbreak( btStream* bts, char *cset, char *bset, char *buf, int bufsz) 
{
    char s[1];
    int res;
    int len=0;

    while (len < bufsz) {
	res = bts_read(bts, s, 1);
	if (res) return res;
	if ( strchr( bset, *s)) break;
	if ( !strchr( cset, *s)) return 1; 
	buf[len++]=*s;
    }
    if (len == bufsz) return 1;
    buf[len]=0; 
    return 0;
}
