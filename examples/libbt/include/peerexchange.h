/*
 *  peerexchange.h
 *  Niagara
 *
 *  Created by Ryan Walklin on 24/05/07.
 *  Copyright 2007 Ryan Walklin. All rights reserved.
 *
 *  Released under the GNU GENERAL PUBLIC LICENCE.
 *
 */

#include <netinet/in.h>
#include "types.h"

// Message defines
#define BT_MSG_EXTENDED 20
#define BT_EXT_HANDSHAKE 0
#define BT_EXT_UTPEX 1

// Def to 1 for debug stderr output
#define PEX_DEBUG 1

enum {
    TRACKER=0,
    CACHE,
    PEX};

typedef struct btPeerAddress {
    struct in_addr ip; // ip address in network order
    uint16_t port;
} btPeerAddress;

typedef struct btPeerCache {
    int len;
    struct btPeerAddress **address;
    int allocated;
} btPeerCache;

struct btDownload;
struct btPeer;

// Peer Exchange
int sendExtendedHandshake(struct btPeer *peer, int listenPort, int privateflag);
int buildDictFromCurrentPeersForPeer(btDict *targetDict, struct btPeer *peer);
int sendPeerExchange(struct btDownload *dl, struct btPeer *peer);

// Peer Cache
btPeerCache *btPeerCache_Create(btPeerCache *cache);
int btPeerCache_AddPeer(btPeerCache *cache, struct btPeer *peer);
int btPeerCache_GetPosition(btPeerCache *cache, struct btPeer *peer);
int btPeerCache_DelPeer(btPeerCache *cache, int position);
void btPeerCache_Destroy(btPeerCache *cache);

