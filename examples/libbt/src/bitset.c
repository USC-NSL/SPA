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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include "bitset.h"
#include "random.h"
#include "util.h"

kBitSet* kBitSet_create( kBitSet *set, int nbits) {
    int bytes = (nbits+7)/8;
    if (nbits %8) bytes++;
    if (!set) {
	set = btcalloc( 1, sizeof(kBitSet));
    } 
    set->nbits = nbits;
    set->bits = btcalloc( bytes, 1);
    return set;
}

kBitSet* kBitSet_createCopy( kBitSet *copy, kBitSet *orig) {
    int bytes;
    int i;
    kBitSet_create( copy, orig->nbits);
    bytes = (orig->nbits+7)/8;
    for (i = 0; i<bytes; i++) {
	copy->bits[i] = orig->bits[i];
    }
    return copy;
}

void kBitSet_finit( kBitSet *set) {
    if (set && set->bits) {
	btfree(set->bits);
	set->bits = NULL;
    }
}

void kBitSet_readBytes( kBitSet *set, char* bits, int len) {
    int i;
    int bytes = (set->nbits+7)/8;
    DIE_UNLESS(len <= bytes);
    for (i=0; i<len; i++) {
	set->bits[i] = bits[i];
    }
}

void bs_set( kBitSet *set, int bit) {
    int byte = bit/8;
    int mask = 0x80 >> (bit %8);
    set->bits[byte] |= mask;
}

void bs_clr( kBitSet *set, int bit) {
    int byte = bit/8;
    int mask = 0x80 >> (bit %8);
    set->bits[byte] &= ~mask;
}

int bs_isSet( kBitSet *set, int bit) {
    int byte = bit/8;
    int mask = 0x80 >> (bit %8);
    return (set->bits[byte] & mask) != 0;
}

void bs_and( kBitSet *res, kBitSet *a, kBitSet *b) {
    int n;
    int bytes = (a->nbits+7)/8;
    DIE_UNLESS(a->nbits == b->nbits);
    DIE_UNLESS(res->nbits == b->nbits);
    for (n=0; n<bytes; n++) {
	res->bits[n] = a->bits[n] & b->bits[n];
    }
}

void bs_not( kBitSet *res, kBitSet *a) {
    int n;
    int bytes = (a->nbits+7)/8;
    DIE_UNLESS(res->nbits == a->nbits);
    for (n=0; n<bytes; n++) {
	res->bits[n] = ~a->bits[n];
    }
}

#if WIN32
char bstest_mask[] = {
		'\xff', '\x80', '\xc0', '\xe0', '\xf0', '\xf8', '\xfc', '\xfe'
	};
#else
char bstest_mask[] = {
	0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
    };
#endif

int bs_isEmpty( kBitSet *a) {
    int n;
    int bytes = (a->nbits+7)/8;
    for (n=0; n<bytes-1; n++) {
	if (a->bits[n]) return 0;
    }
    if (a->bits[bytes-1] & bstest_mask[a->nbits %8]) return 0;
    return 1;
}

int bs_isFull( kBitSet *a) {
    int res;
    kBitSet c;

    kBitSet_create( &c, a->nbits);
    bs_not( &c, a);
    res = bs_isEmpty( &c); 
    kBitSet_finit( &c);
    return res;
}

int bs_firstClr( kBitSet *a) {
    int i;
    for (i=0; i<a->nbits; i++) {
	if (!bs_isSet( a, i)) return i;
    }
    return 0;
}

void bs_setRange( kBitSet *set, int start, int endplus1) {
    int bit;
    for (bit = start; bit<endplus1; bit++) {
	if ((bit & 7) == 0 && (bit + 8)<endplus1) {
	    int byte = bit/8;
            set->bits[byte] = 0xff;
	    bit += 7;
	} else {
	    bs_set( set, bit);
	}
    }
}

void bs_getSparseSet( kBitSet *a, int **data, int *len) {
    int l=0, m=0;
    int *d = NULL;
    int n;
    for (n=0; n<a->nbits; n++) {
	if (bs_isSet( a, n)) {
	    if (l >= m) { m += 10; d = btrealloc( d, m * sizeof(int)); }
	    d[l] = n;
	    l++;
	}
    }
    *data = d;
    *len = l;
}

int bs_countBits( kBitSet *a) {
    int n;
    int count = 0;
    for (n=0; n<a->nbits; n++) {
	if (bs_isSet( a, n)) {
	    count++;
	}
    }
    return count;
}

int bs_hasInteresting( kBitSet *reqs, kBitSet *peer, kBitSet *intr) {
    /*
     * reqs is the list of blocks already requested from other peers
     * peer is the list of blocks available at this peer
     * intr in the list of block that I'm interested in downloading
     *
     * returns the true iff there is at least one block that the peer
     * has, but isn't already requested.
     */
    kBitSet r; 
    int interesting = 1;

    kBitSet_create( &r, peer->nbits);
    bs_not( &r, reqs);
    bs_and( &r, &r, peer);
    bs_and( &r, &r, intr);
    if (bs_isEmpty( &r)) interesting=0;
    kBitSet_finit( &r);

    return interesting;
}

int bs_pickblock( kBitSet *reqs, kBitSet *peer, kBitSet *intr) {
    /*
     * reqs is the list of blocks already requested from other peers
     * peer is the list of blocks available at this peer
     * intr in the list of block that I'm interested in downloading
     *
     * returns the block number of a block to request from this peer,
     * or -1 if there are no blocks to request.
     */
    kBitSet r; 
    int *picks;
    int npicks;
    int block;
    kBitSet_create( &r, peer->nbits);
    bs_not( &r, reqs);
    bs_and( &r, &r, peer);
    bs_and( &r, &r, intr);
    if (bs_isEmpty( &r)) return -1;
#if 0
    bs_dump( "reqs", reqs);
    bs_dump( "peer", peer);
    bs_dump( "intr", intr);
    bs_dump("r", &r);
#endif
    bs_getSparseSet( &r, &picks, &npicks);
    DIE_UNLESS(npicks != 0);
    block = picks[ rnd(npicks)];
    btfree(picks);
    kBitSet_finit( &r);
    return block;
}

void bs_dump( char *label, kBitSet *bs) {
#define BPL 24
    int n,m;
    int bytes = (bs->nbits+7)/8;
    for (n=0; n<bytes; n+=BPL) {
	printf("%s+%05d:", label, n);
	for (m=0; m<BPL && m+n<bytes; m++) {
	    if (m>0 && (m%4)==0) printf(" ");
	    printf ("%02x", (unsigned char)bs->bits[n+m]);
	}
	printf("\n");
    }
    printf("\n");
}
