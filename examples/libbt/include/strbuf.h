#ifndef __DR1STRBUF__H
#define __DR1STRBUF__H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#ifndef min
#define min(x,y) ((x)<(y)?(x):(y))
#endif



/*-------------------------------------------------------------------
 * kStringBuffer
 *
 *    The structure holds a dynamically expanding string as it is 
 *    being printed to
 */

typedef struct kStringBuffer kStringBuffer;

struct kStringBuffer {
    int bufsize;
    int cpos;
    char *buf;
};


/*-------------------------------------------------------------------
 * kStringBuffer_create
 *
 *    Initialize a new stringbuffer.  
 *
 *  PARAMETERS:
 *    sb   If non-null sb points to a string buffer to be initialized
 *
 *  RETURNS:
 *    The initialized string buffer, newly malloc'd if the 'sb' parameter
 *    is NULL.
 *
 *  SIDE EFFECTS:
 */
kStringBuffer*
kStringBuffer_create( kStringBuffer *sb);

/*-------------------------------------------------------------------
 * kStringBuffer_finit
 *
 *    De-Initialize a stringbuffer.  
 *
 *  PARAMETERS:
 *    sb   If non-null sb points to a string buffer to be finalized
 *
 *  RETURNS:
 *    void
 *
 *  SIDE EFFECTS:
 *    Frees the string buffer's malloc'd data, and zeroes the string
 *    buffer structure.  The caller is responsible for freeing the
 *    structure itself (if it was allocated space to start with.)
 */
void
kStringBuffer_finit( kStringBuffer *sb);

/*-------------------------------------------------------------------
 * sbclear
 *
 *    Clear all data from the buffer, returning it to its just 
 *    initialized state.
 *
 *  PARAMETERS:
 *    sb   StringBuffer
 *
 *  RETURNS:
 *
 *  SIDE EFFECTS:
 */
void sbclear( kStringBuffer *sb);


/*-------------------------------------------------------------------
 * k
 *
 *    The method sbputc adds a character to a string buffer.  It
 *    expands the string buffer if the character would be added 
 *    beyond the end of the array.
 *
 *  PARAMETERS:
 *
 *  RETURNS:
 *
 *  SIDE EFFECTS:
 */

int 
sbputc( kStringBuffer *sb, char c); 


/*-------------------------------------------------------------------
 * sbstrcat
 *
 *    Add string to a string buffer
 *
 *  PARAMETERS:
 *     sb   String buffer to modify
 *     str  String to add to the buffer
 *
 *  RETURNS:
 *
 *  SIDE EFFECTS:
 */

int
sbstrcat( kStringBuffer *sb, char *str);

/*-------------------------------------------------------------------
 * sbcat
 *
 *    Add string to a string buffer
 *
 *  PARAMETERS:
 *     sb   String buffer to modify
 *     str  String to add to the buffer
 *     len  Length of the string to add
 *
 *  RETURNS:
 *
 *  SIDE EFFECTS:
 */

int
sbcat( kStringBuffer *sb, char *str, int len);

/*-------------------------------------------------------------------
 * sbtail
 *
 *    Remove text from a buffer, leaving only the tail
 *
 *  PARAMETERS:
 *     sb          String buffer to modify
 *     first_char  First character of the revised buffer
 *
 *  RETURNS:
 *
 *  SIDE EFFECTS:
 */

int
sbtail( kStringBuffer *sb, int first_char);

/*-------------------------------------------------------------------
 * sbindex
 *
 *    Find a character in the buffer
 *
 *  PARAMETERS:
 *     sb   String buffer to modify
 *     ch   Character to look for
 *
 *  RETURNS:
 *     Index of the character matching 'ch', or -1 if not found.
 *
 *  SIDE EFFECTS:
 */

int
sbindex( kStringBuffer *sb, int ch);
#endif /* __DR1STRBUF__H */
