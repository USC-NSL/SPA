#ifndef __BTTYPES__H
#define __BTTYPES__H
#include <assert.h>
#include "util.h"
typedef enum btType { BT_STRING, BT_INTEGER, BT_LIST, BT_DICT } btType;
typedef struct btObject {
    btType t;
    /*... */
} btObject;

#define BTOBJECT(x) (&(x)->parent)
#define BTINTEGER(x) (btInteger_cast(x))
#define BTSTRING(x) (btString_cast(x))
#define BTLIST(x) (btList_cast(x))
#define BTDICT(x) (btDict_cast(x))

typedef struct btInteger {
    btObject parent;
    int allocated; /* really a bool */
    _int64 ival;
} btInteger;

typedef struct btString {
    btObject parent;
    int allocated; /* really a bool */
    int len;
    int bufsize;
    char *buf;
} btString;

typedef struct btList {
    btObject parent;
    int allocated;
    int len;
    int listsize;
    btObject **list;
} btList;

typedef struct btDict {
    btObject parent;
    int allocated;
    int len;
    int dictsize;
    btString *key;
    btObject **value;
} btDict;

typedef struct btDictIt {
    btDict *d;
    int idx;
} btDictIt;

/* Integer */
btInteger* btInteger_cast( btObject *o) ;
btInteger* btInteger_create( btInteger *i); 
btInteger* btInteger_create_int( btInteger *i, int ival); 
void btInteger_destroy( btInteger *i) ;

/* String */
btString* btString_cast( btObject *o) ;
btString* btString_create( btString *s) ;
btString* btString_create_str( btString *s, char *init) ;
btString* btString_create_buf( btString *s, char *initbuf, int buflen) ;
void btString_destroy( btString *s) ;
char *btString_buf( btString *s) ;
int btString_len( btString *s) ;
int btString_cmp( btString *a, btString *b) ;
void btString_setbuf( btString *s, char *buf, int len) ;

/* List */
btList* btList_cast( btObject *o) ;
btList* btList_create( btList *buf) ;
void btList_destroy( btList *buf) ;
int btList_add( btList *_this, btObject *v) ;
btObject* btList_index( btList *_this, int idx) ;

/* Dict */
btDict* btDict_cast( btObject *o) ;
btDictIt* btDictIt_create( btDictIt *buf, btDict *d) ;
btDictIt* btDict_iterator( btDict *d) ;
int btDictIt_hasNext( btDictIt *i) ;
btString* btDictIt_first( btDictIt *i) ;
btString* btDictIt_next( btDictIt *i) ;
btDict* btDict_create( btDict *buf) ;
void btDict_destroy( btDict *_this) ;
int btDict_add( btDict *_this, btString* k, btObject* v) ;
btObject* btDict_find( btDict *_this, btString* k) ;

/* btObject */
int btObject_sizeof( btObject *b) ;
int btObject_destroy( btObject *b) ;
btObject* btObject_val( btObject *b, char *path);
void btObject_dump( int depth, btObject *o);
#endif
