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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <stdio.h>
#include "types.h"

#define QUANTIZE(n) n
void btObject_dump( int d, btObject *b);
/* Integer */
btInteger* btInteger_cast( btObject *o) {
    if (o==NULL) return NULL;
    if (o->t != BT_INTEGER) return NULL;
    DIE_UNLESS(o->t == BT_INTEGER);
    return (btInteger*)o;
}

void btInteger_dump( int d, btInteger *i) {
    int j;
    for (j=0; j<d*3; j++) putchar(' ');
    printf("%p(INTEGER)=%lld\n", i, i->ival);
}

btInteger* btInteger_create( btInteger *i) {
    int allocated = 0;
    if (i == NULL) { i=btmalloc(sizeof( btInteger)); allocated = 1;}
    memset( i, 0, sizeof(btInteger));
    i->allocated = allocated;
    i->parent.t = BT_INTEGER;
    return i;
}

btInteger* btInteger_create_int( btInteger *i, int ival) {
    btInteger *ret = btInteger_create( i);
    ret->ival = ival;
    return ret;
}

void btInteger_destroy( btInteger *i) {
    if (i && i->allocated) btfree(i);
}

/* String */
btString* btString_cast( btObject *o) {
    if (o==NULL) return NULL;
    DIE_UNLESS(o->t == BT_STRING);
    return (btString*)o;
}

void btString_dump( int d, btString *s) {
    int i;
    for (i=0; i<d*3; i++) putchar(' ');
    printf("%p(STRING)=(%d)'", s, s->len);
    for (i=0; i<s->len && i<32; i++) {
	int c = s->buf[i];
	if (c < 32 || c>=127) {
	    putchar('?');
	    /* printf("\\x%02x", (unsigned char)c); */
	} else {
	    putchar(c);
	}
    }
    if (32< s->len) printf("...");
    printf("'\n");
}

btString* btString_create_str( btString *s, char *init) {
    s=btString_create(s);
    btString_setbuf(s,strdup(init),strlen(init));
    return s;
}

btString* btString_create_buf( btString *s, char *initbuf, int buflen) {
    s=btString_create(s);
    char *buf = btmalloc( buflen);
    memcpy( buf, initbuf, buflen);
    btString_setbuf(s,buf,buflen);
    return s;
}

btString* btString_create( btString *s) {
    int allocated = 0;
    if (s == NULL) { s=btmalloc(sizeof( btString)); allocated = 1; }
    memset( s, 0, sizeof(btString));
    s->allocated = allocated;
    s->parent.t = BT_STRING;
    return s;
}

void btString_destroy( btString *s) {
    if (s) {
	if (s->buf) {
	    btfree(s->buf);
	    s->buf = NULL;
	}
	if (s->allocated) btfree(s);
    }
}

int btString_cmp( btString *a, btString *b) {
   int n, along;
   int res;
   if (a->len > b->len) {
       n=b->len; along=1;
   } else {
       n=a->len; 
       if (a->len < b->len) along=-1;
       else along=0;
   }

   res = memcmp(a->buf, b->buf, n);
   if (res == 0) {
      return along;
   }
   return res;
}

char *btString_buf( btString *s) {
    return s->buf;
}

int btString_len( btString *s) {
    return s->len;
}

void btString_setbuf( btString *s, char *buf, int len) {
    s->buf=buf;
    s->len=len;
}

/* List */
btList* btList_cast( btObject *o) {
    if (o==NULL) return NULL;
    DIE_UNLESS(o->t == BT_LIST);
    return (btList*)o;
}

void btList_dump( int d, btList *l) {
    int i,j;
    for (j=0; j<d*3; j++) putchar(' ');
    printf("%p(LIST)={\n", l);
    for (i=0; i<l->listsize; i++) {
        for (j=0; j<d*3; j++) putchar(' ');
	printf("[%d]\n", i);
	btObject_dump( d+1, l->list[i]);
    }
    for (j=0; j<d*3; j++) putchar(' ');
    printf("}\n");
}

btList* btList_create( btList *buf) {
    int allocated = 0;
    if (buf == NULL) { buf=btmalloc(sizeof( btList)); allocated = 1; }
    memset( buf, 0, sizeof(btList));
    buf->allocated = allocated;
    buf->parent.t = BT_LIST;
    return buf;
}

void btList_destroy( btList *buf) {
    int i;
    if (buf) {
	if (buf->list) {
            for (i=0; i < buf->len; i++) {
		btObject_destroy( buf->list[i]);
            }
	    btfree(buf->list);
	    buf->list = NULL;
	}
	if (buf->allocated) btfree (buf);
    }
}

/* the list takes ownership of the object v */
int btList_add( btList *_this, btObject *v) {
    int idx = _this->len++; 
    DIE_UNLESS(_this->parent.t == BT_LIST);
    if (idx >= _this->listsize) {
        int newsize = QUANTIZE(idx+1);
	_this->list = btrealloc( _this->list, sizeof(*_this->list)*newsize);
        _this->listsize = newsize;
    }
    _this->list[idx] = v;
    return 0;
}

btObject *btList_index( btList *_this, int idx) {
    DIE_UNLESS (idx>=0 && idx<_this->len);
    return _this->list[idx];
}

/* Dict */
btDict* btDict_cast( btObject *o) {
    if (o==NULL) return NULL;
    DIE_UNLESS(o->t == BT_DICT);
    return (btDict*)o;
}

void btDict_dump( int e, btDict *d) {
    int i,j;
    for (j=0; j<e*3; j++) putchar(' ');
    printf("%p(DICT)={\n", d);
    for (i=0; i<d->dictsize; i++) {
	btString_dump( e+1, &d->key[i]);
	for (j=0; j<(e+1)*3; j++) putchar(' ');
	printf("=>\n");
	btObject_dump( e+2, d->value[i]);
    }
    for (j=0; j<e*3; j++) putchar(' ');
    printf("}\n");
}

btDictIt* btDictIt_create( btDictIt *buf, btDict *d) {
    if (!buf) {
        buf = btmalloc( sizeof(btDictIt));
        DIE_UNLESS(buf);
    }
    memset(buf, 0, sizeof(btDictIt));
    buf->d = d;
    return buf;
}

btDictIt* btDict_iterator( btDict *d) {
    btDictIt *i = btDictIt_create(NULL, d);
    return i;
}

int btDictIt_hasNext( btDictIt *i) {
    int more;
    more = (i->idx < i->d->len-1);
    return more;
}

btString* btDictIt_first( btDictIt *i) {
    i->idx=0;
    return &i->d->key[i->idx];
}

btString* btDictIt_next( btDictIt *i) {
    if (!btDictIt_hasNext(i)) return NULL;
    i->idx++;
    return &i->d->key[i->idx];
}

btDict* btDict_create( btDict *buf) {
    int allocated = 0;
    if (buf == NULL) { buf=btmalloc(sizeof( btDict)); allocated = 1; }
    memset( buf, 0, sizeof(btDict));
    buf->allocated = allocated;
    buf->parent.t = BT_DICT;
    return buf;
}

void btDict_destroy( btDict *_this) {
    int i;
    if (_this) {
	for (i = 0; i < _this->len; i++) {
	    btString_destroy( &_this->key[i]);
	    if (_this->value[i]) {
		btObject_destroy( _this->value[i]);
		_this->value[i] = NULL;
	    }
	}
	if (_this->key) {
	    btfree(_this->key);
	    _this->key = NULL;
	}
	if (_this->value) {
	    btfree(_this->value);
	    _this->value = NULL;
	}
    }
    if (_this->allocated) btfree (_this);
}

/* takes ownership of the string k, and object v */
int btDict_add( btDict *_this, btString* k, btObject* v) {
    int hi, lo, mid, res, ipos;
    int i;
    int idx = _this->len++; 
    if (idx >= _this->dictsize) {
        int newsize = QUANTIZE(idx+1);
	_this->key = btrealloc( _this->key, sizeof(*_this->key)*newsize);
	_this->value = btrealloc( _this->value, sizeof(*_this->value)*newsize);
        _this->dictsize = newsize;
    }

    /* binary search */
    ipos = hi = idx;
    lo = 0;
    while (lo < hi) {
        mid = (lo + hi) / 2;
	res = btString_cmp(k, &_this->key[mid]);
	if (res == 0) {
	    return -1;  /* key is already in the dictionary */
	}
        if (res < 0) ipos = hi = mid;
	if (res > 0) lo = mid+1;
    }
    for (i=idx-1; i>=ipos; i--) {
        _this->key[i+1] = _this->key[i];
	_this->value[i+1] = _this->value[i];
    }

    btString_create( &_this->key[ipos]);
    btString_setbuf( &_this->key[ipos], k->buf, k->len);
    if (k->allocated) btfree(k);
    _this->value[ipos] = v;
    return 0;
}

btObject* btDict_find( btDict *_this, btString* k) {
    int hi, lo, mid, res;

    /* binary search */
    hi = _this->len;
    lo = 0;
    while (lo < hi) {
        mid = (lo + hi) / 2;
	res = btString_cmp(k, &_this->key[mid]);
	if (res == 0) return _this->value[mid];
        if (res < 0) hi = mid;
	if (res > 0) lo = mid+1;
    }

    return NULL;
}

/* btObject */
int btObject_sizeof( btObject *b) {
    int sz;
    switch(b->t) {
	case BT_LIST:
	    sz=sizeof(btList);
            break;
	case BT_STRING:
	    sz=sizeof(btString);
            break;
	case BT_DICT:
	    sz=sizeof(btDict);
            break;
	case BT_INTEGER:
	    sz=sizeof(btInteger);
            break;
        default:
            sz=0;
    }
    return sz;
}

int btObject_destroy( btObject *b) {
    int sz;
    sz = (int) b->t;
    switch(sz) {
	case BT_LIST:
            btList_destroy((btList*)b);
            break;
	case BT_STRING:
            btString_destroy((btString*)b);
            break;
	case BT_DICT:
            btDict_destroy((btDict*)b);
            break;
	case BT_INTEGER:
            btInteger_destroy((btInteger*)b);
            break;
        default:
            (void)DIE_UNLESS(0==1);
    }
    return sz;
}

void btObject_dump( int d, btObject *b) {
    switch(b->t) {
	case BT_LIST:
            btList_dump( d, (btList*)b);
            break;
	case BT_STRING:
            btString_dump( d, (btString*)b);
            break;
	case BT_DICT:
            btDict_dump( d, (btDict*)b);
            break;
	case BT_INTEGER:
            btInteger_dump( d, (btInteger*)b);
            break;
        default:
            DIE_UNLESS(0==1);
    }
    fflush(stdout);
}

btObject *btObject_val( btObject *o, char *index) {
    char *cpos = index;
    char *buf;
    int len;

    for(;;) {
	while( *cpos != '/' && *cpos) cpos++;
	len = cpos-index;
	buf=btmalloc(len+1);
	memcpy(buf, index, len);
	buf[len]=0;

	if (o->t == BT_LIST) {
	    int idx = atoi(buf);
	    btList *l=BTLIST(o);
	    o=btList_index(l,idx);
	} else if (o->t == BT_DICT) {
	    btString k;
	    btDict *d=BTDICT(o);
	    btString_create_str(&k, buf);
	    o=btDict_find(d,&k);	
	    btString_destroy(&k); 
	} else {
	    btfree(buf);
	    return NULL;
	}
	btfree(buf);
        if (!*cpos) break;
        index=++cpos;
    }
    return o;
}
