#ifndef __BITSET__H
#define __BITSET__H

typedef struct kBitSet {
    int nbits;
    char *bits;
} kBitSet;

kBitSet* kBitSet_create( kBitSet *set, int nbits) ;
kBitSet* kBitSet_createCopy( kBitSet *set, kBitSet *orig);
void kBitSet_finit( kBitSet *set);
void kBitSet_readBytes( kBitSet *set, char* bytes, int len) ;

void bs_set( kBitSet *set, int bit) ;
void bs_setRange( kBitSet *set, int start, int endplus1);

void bs_clr( kBitSet *set, int bit) ;

int bs_isSet( kBitSet *set, int bit) ;
int bs_firstClr( kBitSet *set);
int bs_isFull( kBitSet *set);
int bs_isEmpty( kBitSet *set);
int bs_countBits( kBitSet *set) ;
   /*
    * REturns the number of 1 bits in the set
    */

int bs_pickblock( kBitSet *req, kBitSet *peer, kBitSet *intr);
   /*
    * randomly picks a block to get from a peer, without asking for any
    * that have already been requested elsewhere and are listed in 'req'. 
    */
int bs_hasInteresting( kBitSet *req, kBitSet *peer, kBitSet *intr);
   /*
    * returns true iff the peer has at least one block that hasn't 
    * already been requested.
    */

void bs_dump( char *label, kBitSet *set);
#endif
