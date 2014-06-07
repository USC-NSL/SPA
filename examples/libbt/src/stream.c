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

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#if WIN32
#   include <winsock2.h>
#   include <io.h>
#   define write( s, buf, size) send( s, buf, size, MSG_OOB)
#   undef EAGAIN
#   define EAGAIN WSAEWOULDBLOCK
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   if HAVE_UNISTD_H
#       include <unistd.h>
#   endif
#endif
#include <sys/types.h>
#include "bterror.h"
#include "stream.h"
#include "util.h"
#undef DEBUG_STREAM

kStream* kStream_create( kStream *str, int sd) {
    if (!str) str = btcalloc( 1, sizeof(kStream));
    str->fd = sd;
    kStringBuffer_create( &str->ibuf);
    kStringBuffer_create( &str->obuf);
    return str;
}

void
kStream_finit( kStream *str) {
    kStringBuffer_finit( &str->ibuf);
    kStringBuffer_finit( &str->obuf);
}

#ifdef DEBUG_STREAM
static void putch( int c) {
    if (c < 32 || c >= 127) printf("?");
    else putchar(c);
}

static void hexdump( char *buf, int len) {
    int addr, i,j;
    for (addr = 0; addr < len; addr+=20) {
	printf("%08x: ", addr);
	for (i = 0; i < 20; i+= 4) {
	    for (j = 0; j < 4; j++) {
		if (addr+i+j >= len) {
		    printf("..");
		} else {
		    printf("%02x", (unsigned char)buf[addr+i+j]);
		}
	    }
	    printf(" ");
	}
	printf(" |");
	for (i = 0; i < 20; i++) {
	    if (addr+i > len) {
		putch('.');
	    } else {
		putch((unsigned char)buf[addr+i+j]);
	    }
        } 
	printf("|\n");
    }
}
#endif

int kStream_read( kStream *str, char *buf, int max) {
    /* unbuffered stream reader */
    int nread;

    nread = recv( str->fd, buf, max, 0);
#if 0
    printf("%d: stream: Read %d bytes\n", str->fd, nread); 
#endif
    if (nread > 0) {
	str->error_count=0;
	str->read_count += nread;
#if 0
	printf( "%d: read %d bytes\n", str->fd, str->read_count);
#endif
    }  else {
	if (nread == 0) {
	    /* bug in linux implementation of recv()? */
	    errno = EAGAIN;
	    nread = -1;
	} 
	
	if (nread < 0) {
	    if (errno == EAGAIN) {
		/* cap the number of EAGAINs on any socket 
		 * select shouldn't pick us when there is no data */
		str->error_count++;
/*	    	printf("Read 0 (count=%d)\n", str->error_count); */
		if (str->error_count > 10) {
		    errno = BTERR_NETWORK_ERROR;
		}
	    } 
	    str->error = errno;
	    if (errno != EAGAIN) {
		bts_perror(errno, "read");
		fprintf(stderr, "%d: Read error %s\n", str->fd, bts_strerror( errno));
	    }
	} 
    }

#ifdef DEBUG_STREAM
    if (nread>0) {
	printf("%d: read> (%d bytes)\n", str->fd, nread);
	hexdump( buf, nread);
    }
#endif
    return nread;
}


int kStream_write( kStream *str, char *buf, int size) {
    /* unbuffered stream writer */
    int nwrite;

#ifdef DEBUG_STREAM
    printf("%d: write> (%d bytes)", str->fd, size);
    hexdump( buf, size);
    printf("'\n");
#endif

    nwrite = write( str->fd, buf, size);

    if (nwrite < 0) {
	str->error = errno;
	if (errno != EAGAIN) {
	    printf("%d: Write error %s\n", str->fd, bts_strerror( errno));
	}
    } else {
	str->write_count += nwrite;
#if 0
	printf("%d: written %d bytes\n", str->fd, str->write_count);
#endif
	if (nwrite < size) {
	    str->error = EAGAIN;
	}
    }
    return nwrite;
}

int kStream_in_addr( kStream *str) {
    return str->read_count - str->ibuf.cpos;
}

int kStream_out_addr( kStream *str) {
    return str->write_count + str->obuf.cpos;
}

int kStream_fpeek( kStream *str, char *buf, int size) {
    char tbuf[1024];
    int total = 0;
    int nread;
    int len;
    int err;

    /* Buffer ahead to the next newline */
    len = kStream_iqlen( str);
    while (len<size) {
        /* loop until all pending data has been read, or 'size' bytes are available  */
	nread = kStream_read( str, tbuf, sizeof( tbuf));
#if 0
	printf("stream: fpeek got %d bytes for %d total\n", nread, nread+len); 
#endif
	if (nread < 0) {
	    /* break if no more data available */
	    return nread;
	}
	total += nread;
	err = sbcat( &str->ibuf, tbuf, nread);
	if (err) return -1;
	len = kStream_iqlen( str);
    }

    /* Got enough data, now copy to the buffer */
    memcpy( buf, str->ibuf.buf, size);
    return size;
}

int kStream_fread( kStream *str, char *buf, int size) {
    int read = kStream_fpeek( str, buf, size);
    if (read>0) {
	sbtail( &str->ibuf, size);
    }
    return read;
}

int kStream_clear( kStream* str) {
    /* return 0 on success */
    sbclear( &str->obuf);
    return 0;
}

int kStream_flush( kStream* str) {
    /* return number of bytes still queued, or -1 on error */
    int nwrite;
    int res;
    nwrite = kStream_write( str, str->obuf.buf, str->obuf.cpos);
    if (nwrite > 0) {
	sbtail( &str->obuf, nwrite);
    } 
    res = str->obuf.cpos;
    if (nwrite <= 0 && str->error != EAGAIN) res = -1;
    return res;
}

int kStream_fwrite( kStream *str, char *buf, int len) {
    /* return number of bytes still queued, or -1 on error */
    if (sbcat( &str->obuf, buf, len)) {
        str->error = ENOMEM;
        return -1;
    }
    return kStream_flush( str);
}

int kStream_iqlen( kStream *str) {
    return str->ibuf.cpos;
}

int kStream_oqlen( kStream *str) {
    return str->obuf.cpos;
}
