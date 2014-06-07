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
#include "util.h"

#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include "strbuf.h"
#ifndef max
#define max(x,y) ((x)>(y)?(x):(y))
#endif
#if WIN32
#   define vsnprintf _vsnprintf
#endif

kStringBuffer*
kStringBuffer_create( kStringBuffer* _sb) {
    kStringBuffer *sb;

    if (_sb) {
	sb = _sb;
    } else {
	sb = btmalloc( sizeof( kStringBuffer));
    }
    memset( sb, 0, sizeof( kStringBuffer));

    return sb;
}

void
kStringBuffer_finit( kStringBuffer* sb) {
    if (sb) {
	if ( sb->buf)
	    btfree( sb->buf);
	sb->buf = NULL;
	sb->bufsize = 0;
	sb->cpos = 0;
    }
}

int
kStringBuffer_grow( kStringBuffer *sb, int len) {
    sb->bufsize += len;
    sb->buf = btrealloc( sb->buf, sb->bufsize);
    DIE_UNLESS(sb->buf);
    return 0;
}

void
sbclear( kStringBuffer* sb) {
    kStringBuffer_finit( sb);
}

int 
sbputc( kStringBuffer *sb, char c) {
    if (sb->cpos + 1 >= sb->bufsize) {
	kStringBuffer_grow( sb, 1024);
	if (!sb->buf) return -1;
    }
    sb->buf[sb->cpos++] = c;
    sb->buf[sb->cpos] = 0;
    return 0;
}

int
sbstrcat( kStringBuffer *sb, char *str) {
    return sbcat( sb, str, strlen(str));
}

int
sbcat( kStringBuffer *sb, char *str, int len) {
    DIE_UNLESS( len >= 0);
    if (sb->cpos + len >= sb->bufsize) {
	kStringBuffer_grow( sb, max( len+1, 1024));
	if (!sb->buf) return -1;
    }
    memcpy(sb->buf + sb->cpos, str, len);
    sb->cpos += len;
    sb->buf[sb->cpos] = 0;
    return 0;
}

int
sbtail( kStringBuffer *sb, int start_char) {
    DIE_UNLESS( start_char >= 0 && start_char <= sb->cpos);
    if (start_char == 0) return 0;
    if (start_char < sb->cpos) {
	memmove( sb->buf, sb->buf + start_char, sb->cpos - start_char);
    }
    sb->cpos -= start_char;
    sb->buf[sb->cpos] = 0;
    return 0;
}

int 
sbindex( kStringBuffer *sb, int ch) {
    int count=0;
    char *s = sb->buf;
    while (count < sb->cpos && *s != ch) s++, count++;
    if (count < sb->cpos) return count;
    return -1;
}
