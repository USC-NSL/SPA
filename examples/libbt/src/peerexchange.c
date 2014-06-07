/*
 *  peerexchange.c
 *  Niagara
 *
 *  Created by Ryan Walklin on 24/05/07.
 *  Copyright 2007 Ryan Walklin. All rights reserved.
 *
 *  Released under the GNU LIBRARY GENERAL PUBLIC LICENCE.
 *
 */

#include "config.h"
#include "string.h"
#include "peerexchange.h"
#include "context.h"
#include "bts.h"
#include "benc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>


#pragma mark Extended Message

int sendExtendedHandshake(btPeer *peer, int listenPort, int privateflag)
{
    int res;
    btDict *extendedHandshakeDict, *messageDict;
    btInteger *i;
    btString *bencKey;
    btStream *extendedHandshakeStream;
#ifdef NIAGARA_COCOA
    btString *bencValue;
    char *versionString;
#endif
    
    char type = BT_MSG_EXTENDED;
    int32_t nslen;
    
    // Create handshake dictionary
    extendedHandshakeDict = btDict_create(NULL);
    
    // Set encryption string
    bencKey = btString_create_str(NULL, "e");
    i = btInteger_create_int(NULL, 0);
    btDict_add(extendedHandshakeDict, bencKey, BTOBJECT(i));
    
    // Create version string 
#ifdef NIAGARA_COCOA
    asprintf(&versionString, "%s %s", NG_NAME, NG_VERSION);
    bencValue = btString_create_str(NULL, versionString);
    bencKey = btString_create_str(NULL, "v");
    btfree(versionString);
    btDict_add(extendedHandshakeDict, bencKey, BTOBJECT(bencValue));
#endif
    
    // Listen port
    bencKey = btString_create_str(NULL, "p");
    i = btInteger_create_int(NULL, listenPort);
    btDict_add(extendedHandshakeDict, bencKey, BTOBJECT(i));
    
    // Message
    messageDict = btDict_create(NULL);
    {
	bencKey = btString_create_str(NULL, "ut_pex");
	i = btInteger_create_int(NULL, privateflag ? 0 : BT_EXT_UTPEX);
	btDict_add(messageDict, bencKey, BTOBJECT(i));
    }
    bencKey = btString_create_str(NULL, "m");
    btDict_add(extendedHandshakeDict, bencKey, BTOBJECT(messageDict));
    
    // create stream    
    extendedHandshakeStream = bts_create_strstream();
    benc_put_object(extendedHandshakeStream, BTOBJECT(extendedHandshakeDict));
    
    struct btstrbuf extendedHandshakeBuf = bts_get_buf(extendedHandshakeStream);
    nslen = htonl(extendedHandshakeBuf.len + 2); // 1 byte for Extended ID and 1 for Message ID
    
    res = kStream_fwrite( &peer->ios, (void *)&nslen, sizeof(nslen));
    if (res < 0) return -1;
    res = kStream_fwrite( &peer->ios, &type, 1);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, "\0", 1);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, extendedHandshakeBuf.buf, extendedHandshakeBuf.len);
    if (res < 0) return -3;
    
#if PEX_DEBUG
    fprintf(stderr, "Sending message to peer %s:\n", inet_ntoa( peer->sa.sin_addr));
    hexdump( extendedHandshakeBuf.buf, extendedHandshakeBuf.len);
#endif
    
    btDict_destroy(extendedHandshakeDict);
    bts_destroy(extendedHandshakeStream);
    
    return res;
}

int buildDictFromDownloadForPeer(btDownload *dl, btDict *targetDict, btPeer *peer)
{
    int i;
    int count;
    btPeerCache *added;
    btString *bencKey, *bencValue;
    char *addedstring, *droppedstring;
    
    added = btPeerCache_Create(NULL);
    
    for (i=0; i<dl->peerset.len; i++)
    {
        btPeer *p = dl->peerset.peer[i];
        int position = -1;
        if (p->state != PEER_GOOD || p->sa.sin_port == 0) continue; // only send good peers with known ports
	position = btPeerCache_GetPosition( peer->last_exchange, p);
        if (position == -1)
        {
            // peer new, add to added dict
            btPeerCache_AddPeer(added, p);
        } else {
            // peer old, remove from removed dict if present
            btPeerCache_DelPeer( peer->last_exchange, position);
        }
    }
    
    // added.f
    bencKey = btString_create_str(NULL, "added.f");
    bencValue = btString_create_str(NULL, "");
    btDict_add(targetDict, bencKey, BTOBJECT(bencValue));

    // Set up added key
    bencKey = btString_create_str(NULL, "added");

    addedstring = btcalloc(1, (sizeof(unsigned long) + sizeof(uint16_t)) * added->len);
    count = 0;
    for (i=0; i<added->len; i++)
    {
        if (added->address[i] == NULL) continue;
        memcpy(addedstring+(count*6), &added->address[i]->ip.s_addr, 4);
        memcpy(addedstring+(count*6)+4, &added->address[i]->port, 2);
	count++;
    }
    
    if (count > 0) {
	bencValue = btString_create_buf(NULL, addedstring, count * 6);
	btDict_add(targetDict, bencKey, BTOBJECT(bencValue));
    }
    btfree( addedstring);
    
    // dropped
    bencKey = btString_create_str(NULL, "dropped");
    droppedstring = btcalloc(1, (sizeof(unsigned long) + sizeof(uint16_t)) * TORRENT_MAXCONN);
    count = 0;
    for (i=0; i<peer->last_exchange->len; i++)
    {
        if (peer->last_exchange->address[i] == NULL) continue;
        memcpy(droppedstring+(i*6), &peer->last_exchange->address[i]->ip.s_addr, 4);
        memcpy(droppedstring+(i*6)+4, &peer->last_exchange->address[i]->port, 2);
	count ++;
    }

    if (count > 0) {
	bencValue = btString_create_str(NULL, droppedstring);
	btDict_add(targetDict, bencKey, BTOBJECT(bencValue));
    }
    btfree( droppedstring);
    
    // Copy added to last exchange
    btPeerCache_Destroy(peer->last_exchange);
    peer->last_exchange = added;
    
    return 0;
}

int sendPeerExchange(btDownload *dl, btPeer *peer)
{
    int res;
    btDict *exchangeDict;
    btStream *exchangeStream;
    
    char type = BT_MSG_EXTENDED;
    char msgindex = BT_EXT_UTPEX;
    int32_t nslen;
    
    // Get Dictionary
    exchangeDict = btDict_create(NULL);
    buildDictFromDownloadForPeer(dl, exchangeDict, peer);
    
    // Create Stream    
    struct btstrbuf exchangeBuffer;
    exchangeStream = bts_create_strstream();
    benc_put_object(exchangeStream, BTOBJECT(exchangeDict));
    exchangeBuffer = bts_get_buf(exchangeStream);
    
    // Send message
    nslen = htonl(exchangeBuffer.len + 2);
    
    res = kStream_fwrite( &peer->ios, (char *)&nslen, sizeof(int32_t));
    if (res < 0) return -1;
    res = kStream_fwrite( &peer->ios, &type, 1);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, &msgindex, 1);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, exchangeBuffer.buf, exchangeBuffer.len);
    if (res < 0) return -3;
    
    // Clean up
    time(&peer->pex_timer);
    
#if PEX_DEBUG
    fprintf(stderr, "Sending message to peer %s:\n", inet_ntoa( peer->sa.sin_addr));
    hexdump( exchangeBuffer.buf, exchangeBuffer.len);
#endif
    
    btDict_destroy(exchangeDict);
    bts_destroy(exchangeStream);
    
    return res;
}

#pragma mark -
#pragma mark Peer Exchange Cache

btPeerCache* btPeerCache_Create(btPeerCache *cache) 
{
    if (!cache) 
    {
        cache = (btPeerCache *)btcalloc(1, sizeof(btPeerCache));
        cache->allocated = 1;
    }
    else
    {
        memset(cache, 0, sizeof(btPeerCache));
        cache->allocated = 0;
    }
    cache->address = NULL;
    return cache;
}

int btPeerCache_GetPosition(btPeerCache *cache, btPeer *peer)
{
    int i, position = -1;
    if (cache->len > 0)
    {
        for (i=0; i<cache->len; i++)
        {
            btPeerAddress *paddress = cache->address[i];
            if (!paddress) continue;
            
            // compare oldpeer with cache by ip and port
	    short peerport = ntohs( peer->sa.sin_port);
            if (memcmp(&paddress->ip, &peer->sa.sin_addr, sizeof(struct in_addr)) == 0 &&
                memcmp(&paddress->port, &peerport, sizeof(uint16_t)) == 0)
            {
                // peer found in cache
                position = i;
#if PEX_DEBUG
                fprintf(stderr, "Peer %s:%i found in array at position %i\n",
                        inet_ntoa(peer->sa.sin_addr),
                        peerport,
                        position);
#endif
                break;
            }
        }
    }
#if PEX_DEBUG
    if (position == -1) fprintf(stderr, "Peer %s:%i not found in array\n", inet_ntoa(peer->sa.sin_addr), ntohs(peer->sa.sin_port));
#endif
    return position;
}


int btPeerCache_AddPeer(btPeerCache *cache, btPeer *peer)
{
    int idx, position = -1;
    struct btPeerAddress *paddress = NULL;
    
    position = btPeerCache_GetPosition(cache, peer);
    if (position != -1)
    {
#if PEX_DEBUG
        fprintf(stderr, "Peer %s alread in array at position %i", inet_ntoa(peer->sa.sin_addr), position);
#endif
        return position;
    }
    
    idx = cache->len++;
    
    // initialise address struct
    paddress = btcalloc(1, sizeof(struct btPeerAddress));
    memcpy(&paddress->ip, &peer->sa.sin_addr, sizeof(struct in_addr));
    paddress->port = ntohs(peer->sa.sin_port);
    
    // Add to cache
    cache->address = btrealloc(cache->address, sizeof(struct btPeerAddress*) * cache->len);
    cache->address[idx] = paddress;
    return 0;
}


int btPeerCache_DelPeer(btPeerCache *cache, int position)
{
    int i;
    
    if (position < 0 || position >= cache->len)
    {
#if PEX_DEBUG
        fprintf(stderr, "Position %i not in peer cache\n", position);
#endif
        return -1;
    }
    
    btPeerAddress *paddress = cache->address[position];
    
    cache->address[position] = NULL;
    btfree(paddress);

    /* shift up the rest */
    for (i=position; i<cache->len-1; i++)
    {
        cache->address[i] = cache->address[i+1];
    }
    cache->len--;
    return 0;
}


void btPeerCache_Destroy(btPeerCache *cache)
{
    int i;
    if (cache) 
    {
        if (cache->address) 
        {
	    for (i=0; i<cache->len; i++) {
		assert( cache->address[i]);
		btfree( cache->address[i]);
	    }
            btfree(cache->address);
            cache->address = NULL;
        }
        if (cache->allocated) btfree(cache);
    }
}
