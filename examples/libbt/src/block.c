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

struct Request {
    int start;
    int end;
} stRequest;

struct Block {
    int start;
    int end;
    struct PeerSet *pset;
} stBlock;

struct PeerSet {
    int npeers;
    struct Peer *peer;
} stPeerSet;

struct Peer {
    char id[20];
    int nreqs;
    struct Request *req;
} stPeer;
    
int stepsize( struct Block *b, int cpos) {
    /* 
     * In the block b, what is the next character to check after cpos?
     */
    
    struct Peer *peer;
    struct Request *req;
    int step, i;
    int minstep = b->end - cpos;

    for (i=0; i< p->npeers; i++) {
        peer = b->pset.peer[i];
        for (j=0; j< peer->nreqs; j++) {
	    req = peer->req[j];
	    if ( cpos >= req->starts && cpos < req->end) {
		step = req->end - cpos;
		if (step < minstep) minstep = step;
	    } else if (cpos < req->start) {
		step = req->start - cpos;
		if (step > 0 && step < minstep) minstep = step;
	    }
	}
    }
    return minstep;
}

int isRequested( struct Peer *peer, int cpos) {
    /*
     * have I yet requested a block containing cpos from this peer?
     */
    int j;
    struct Request *req;
    for (j=0; j< peer->nreqs; j++) {
	req = peer->req[j];
	if ( cpos >= req->starts && cpos < req->end) {
	    return 1;
	}
    }
    return 0;
}

int overlaps( struct Block *b, int cpos) {
    /*
     * how many times have I made requests from this block that contain cpos?
     */
    int depth = 0;

    for (i=0; i< p->npeers; i++) {
	depth += isRequested( &b->pset.peer[i], cpos);
    }

    return depth;
}

struct Request *findRequest( struct Block *b, struct Peer *p, int maxsize) {
    /*
     * what is the best part of this block for me to request?
     */
    int mindepth = 5;
    int minsize = 0;
    struct Request *req = btcalloc(sizeof (struct Request));

    for (cpos = 0; cpos < b->end; cpos += stepsize(b,cpos)) {
        depth=overlaps( b, cpos);
	if (isRequested( p, cpos)) continue;
	if (depth < mindepth || (depth == mindepth && size > minsize)) {
	    req->start = cpos;
	    minsize = min(size, maxsize);
	    req->end = cpos + minsize;
	    mindepth = depth;
	    if (depth == 0 && size >= maxsize) break;
	}
    }

    if (mindepth == 5) { btfree(req); return NULL; }
    return req;
}

int
addRequest( struct Block *b, char peerid[20], struct Request *req) {
    DIE_UNLESS(req);

}

