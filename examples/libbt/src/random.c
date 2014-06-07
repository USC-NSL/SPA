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

#if WIN32
#   include <Rpc.h>
#   include <Rpcdce.h>
#else
#   if HAVE_UUID_UUID_H
#       include <uuid/uuid.h>
#   endif
#   ifndef HAVE_UUID_GENERATE
#       include <stdio.h>
#   endif
#endif
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#   include <strings.h>
#endif
#include <openssl/sha.h>
#include <math.h>


#if !WIN32
void seed_rnd( char *buf, int len) {
    char dg[SHA_DIGEST_LENGTH];
    SHA1( buf, len, dg);
    seed48( (unsigned short*)dg);
}
#endif

void randomid( char *buf, int len) {

#if WIN32
    GUID* newuuid = 0;
    UuidCreate(newuuid);
#else
#   if HAVE_UUID_UUID_H
	uuid_t newuuid;
#   else
	char newuuid[20];
#   endif
#   ifdef HAVE_UUID_GENERATE
    uuid_generate( newuuid);
#   else
    FILE *R;
    R = fopen("/dev/urandom","r");
    fread(newuuid,sizeof(newuuid),1,R);
    fclose(R);
#   endif
#endif

    memset(buf, 0, len);
    if (len > sizeof(newuuid)) {
	memcpy(buf, newuuid, sizeof(newuuid));
    } else {
	memcpy(buf, newuuid, len);
    }
#if !WIN32
    seed_rnd( (char *)newuuid, sizeof(newuuid));
#endif
}


#if !WIN32
int rnd(int randmax) {
    double r = drand48();
    int rval = r*randmax;
    if (rval >= randmax || rval < 0) rval=randmax-1;
    return rval;
}
#endif

double normal( double mean, double sdev) {
/* knuth algorithm P */
    double v1=0, v2=0;
    double s, x1;
    do {
	v1 = drand48() * 2 - 1;
	v2 = drand48() * 2 - 1;
	s = v1+v2;
    } while (s >= 1.0);
    x1 = v1 * sqrt( (-2 * log(s)) / s);

    return (x1 * sdev) + mean;
}
