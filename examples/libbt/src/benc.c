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

#include "benc.h"
#include "bterror.h"

#define MAXSTRING 8*1024*1024 /* sanity check string length */

/**	\brief	The benc_put_string function serializes a btString to 
                the stream 'bts' in BEncoded format.

	\param	bts	a parameter of type struct btStream*
	\param	s	a parameter of type btString *

	\return	int	0 - sucess, 1 - error

	
*/
int benc_put_string( struct btStream* bts, btString *s) {
    int res;
    res = bts_printf(bts, "%d:", btString_len(s));
    res |= bts_write( bts, btString_buf(s), btString_len(s));
    return res;
}

/**	\brief	The benc_get_string function deserializes a btSring 
                from the stream 'bts' using the BEncoding scheme.

	\param	bts	a parameter of type struct btStream*
	\param	s	a parameter of type btString *

	\return	int	0 - success, 1 - some error

	
*/
int benc_get_string( struct btStream* bts, btString *s) {
    int res;
    char ibuf[10];
    char *sbuf;
    int slen;

    res = bts_scanbreak( bts, "0123456789", ":", ibuf, sizeof(ibuf));
    if (res) return res;

    slen= atoi(ibuf);
    assert(slen<MAXSTRING);
    sbuf = btmalloc(slen+1);
    DIE_UNLESS(sbuf);
    if (slen > 0) {
	res = bts_read( bts, sbuf, slen);
    } else if (slen < 0) {
    	errno = BTERR_NEGATIVE_STRING_LENGTH;
	res = -1;
    } else {
	*sbuf = 0;
	res = 0;
    }
    if (res) {
        btfree(sbuf);
      	return res;
    }
    sbuf[slen]=0;

    btString_setbuf(s, sbuf, slen);
    return 0; 
}

/**	\brief	The benc_put_int function serializes a btInteger to the
                stream 'bts' 

	\param	bts	a parameter of type struct btStream *
	\param	i	a parameter of type btInteger *

	\return	int	0 - success, 1 - error

	
*/
int benc_put_int( struct btStream *bts, btInteger *i) {
    int res;

    res = bts_printf(bts, "i%llde", i->ival);
    return res;
}

/**	\brief	The benc_get_int function

	\param	bts	a parameter of type struct btStream *
	\param	i	a parameter of type btInteger *

	\return	int	0 - success, 1 - error

	
*/
int benc_get_int( struct btStream *bts, btInteger *i) {
    int res;
    char c[1];
    char ibuf[25];

    res = bts_read( bts, c, 1);
    if (res) return res;
    if (*c != 'i') return 1;
    res = bts_scanbreak( bts, "0123456789", "e", ibuf, sizeof(ibuf));
    if (res) return res;

#if HAVE_STRTOLL
    i->ival = strtoll( ibuf, NULL, 10);
#else
    i->ival = atoi( ibuf);
#endif
    return 0;
}

/**	\brief	The benc_put_list function serializes a btList and its 
                contents to the stream 'bts'.

	\param	bts	a parameter of type struct btStream *
	\param	a	a parameter of type btList *

	\return	int	0 - success, 1 - error

	
*/
int benc_put_list( struct btStream *bts, btList *a) {
    int res;
    int i;

    res = bts_printf( bts, "l");
    for (i=0; i<a->len; i++) {
        res |= benc_put_object( bts, a->list[i]);
    }
    res |= bts_printf( bts, "e");
    return res;
}

/**	\brief	The benc_get_list function deserializes a list including
 		all list values from the stream 'bts'.

	\param	bts	a parameter of type struct btStream *
	\param	a	a parameter of type btList *

	\return	int	0 - success, 1 - error

	
*/
int benc_get_list( struct btStream *bts, btList *a) {
    int res;
    int c;
    char buf[1];
    
    res = bts_read( bts, buf, 1);
    if (res) return res;
    if (*buf != 'l') return 1;
    c = bts_peek( bts);
    while (c != 'e') {
        btObject *o;
        res = benc_get_object( bts, &o);
        if (res) return 1;
        btList_add( a, o);
        c = bts_peek( bts);
    }
    res = bts_read( bts, buf, 1);
    if (res) return res;
    if (*buf != 'e') return 1;
    return 0;
}

/**	\brief	The benc_put_dict function serializes a btDict to the
		stream 'bts'.  

	\param	bts	a parameter of type struct btStream *
	\param	d	a parameter of type btDict *

	\return	int	0 - success, 1 - error

	
*/
int benc_put_dict( struct btStream *bts, btDict *d) {
    btDictIt i;
    btString *k;
    btObject *v;
    int res = 0;

    btDictIt_create( &i, d);

    res = bts_printf( bts, "d");
    if (res) return res;

    for (k = btDictIt_first(&i); k!=NULL; k = btDictIt_next(&i)) {
        v = btDict_find(d, k);
	res = benc_put_string( bts, k);
	if (res) return res;
        res = benc_put_object( bts, v);
        if (res) return res;
    }

    res = bts_printf( bts, "e");
    return res;
}

/**	\brief	The benc_get_dict function deserializes a btDict from the
		stream 'bts'.

	\param	bts	a parameter of type struct btStream *
	\param	a	a parameter of type btDict *

	\return	int	0 - success, 1 - error

	
*/
int benc_get_dict( struct btStream *bts, btDict *a) {
    int res;
    int c;
    char buf[1];
    
    res = bts_read( bts, buf, 1);
    if (res) return res;
    if (*buf != 'd') return 1;
    c = bts_peek( bts);
    while (c != 'e') {
        btString *k;
        btObject *o;
        k = btString_create( NULL);
        res = benc_get_string( bts, k);
        if (res) return res;
        res = benc_get_object( bts, &o);
        if (res) return res;

        if (btDict_add( a, k, o)) {
	    return 1;
	}
        c = bts_peek( bts);
    }
    res = bts_read( bts, buf, 1);
    if (res) return res;
    if (*buf != 'e') return 1;
    return 0;
}


/**	\brief	The benc_put_object is a virtual serialization function
		that selects the appropriate serialization of a btObject
		by type, and calls that function on the stream 'bts'.

	\param	bts	a parameter of type btStream *
	\param	o	a parameter of type btObject *

	\return	int 	1=failed, 0=success

	
*/
int benc_put_object( btStream *bts, btObject *o) {
    int res=0;
    if (o->t == BT_STRING) {
        res = benc_put_string( bts, (btString*)o);
    } else if (o->t == BT_INTEGER) {
        res = benc_put_int( bts, (btInteger*)o);
    } else if (o->t == BT_LIST) {
        res = benc_put_list( bts, (btList*)o);
    } else if (o->t == BT_DICT) {
        res = benc_put_dict( bts, (btDict*)o);
    } else {
        res=1;
    }
    return res;
}

/**	\brief	The benc_get_object is a virtual deserialization function
		that examines the stream 'bts' to determine the type of
		the next object on the stream, and calls the appropriate
		deserializer for that type.

	\param	bts	a parameter of type btStream *
	\param	*o	a parameter of type btObject *

	\return	int	0 - success, 1 - failed

	
*/
int benc_get_object( btStream *bts, btObject **o) {
    btDict *d;
    btList *l;
    btInteger *i;
    btString *s;
    int res=0;
    int c = bts_peek( bts);

    if (c<0) return -1;
    switch (c) {
        case 'd':        
            d = btDict_create(NULL);
	    res = benc_get_dict(bts, d);
            *o = &d->parent;
	    break;
        case 'l':        
            l = btList_create(NULL);
	    res = benc_get_list(bts, l);
            *o = &l->parent;
	    break;
        case 'i':        
            i = btInteger_create(NULL);
	    res = benc_get_int(bts, i);
            *o = &i->parent;
	    break;
	default:
	    if (c >= '0' && c <= '9') {
                s = btString_create(NULL);
		res = benc_get_string(bts,s);
                *o = &s->parent;
	    } else return -1;
    }
    return res;
}
