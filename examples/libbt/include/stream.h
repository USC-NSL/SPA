#ifndef __KSTREAM__H
#define __KSTREAM__H
#include "strbuf.h"

/*-------------------------------------------------------------------
 * kStream
 *
 *    The structure kStream adds stream semantics to a socket
 */
typedef struct kStream {
    int fd;
    int inputready;
    int error;
    int error_count;
    int read_count;
    int write_count;
    kStringBuffer ibuf;
    kStringBuffer obuf;
} kStream;


/*-------------------------------------------------------------------
 * kStream_create
 *
 *    The method constructs a Stream
 *
 *  PARAMETERS:
 *    str   Stream to intialize, or NULL to calloc the stream
 *    sd    Socket to connect the stream to
 *
 *  RETURNS:
 *    The initialized stream
 *
 *  SIDE EFFECTS:
 */
kStream* 
kStream_create( kStream *str, int sd);

/*-------------------------------------------------------------------
 * kStream_finit
 *
 *    The method destroys a stream
 *
 *  PARAMETERS:
 *    str   Stream to intialize, or NULL to calloc the stream
 *
 *  SIDE EFFECTS:
 */
void
kStream_finit( kStream *str);

/*-------------------------------------------------------------------
 * kStream_read
 *
 *    Unbuffered reader
 *
 *    The method reads at at most 'max' characters from the stream.  
 *    The number of characters read is returned, or -1 is returned on
 *    error.
 *
 *  PARAMETERS:
 *    str    Stream
 *    buf    Buffer to write to
 *    max    Try to fill the buffer this full
 *
 *  RETURNS:
 *    Number of characters read, or -1 on error.  
 *
 *  SIDE EFFECTS:
 *    Sets ios->error to the error code if there is an error.
 */

int kStream_read( kStream *str, char *buf, int max);

/*-------------------------------------------------------------------
 * kStream_write
 *
 *    The method writes 'len' characters to the stream.
 * 
 *    This method is unbuffered.  It may not write all of the 
 *    characters requested depending on how much room is 
 *    available in the output stream.  Use buffered output if
 *    you need to write data and don't want to loop until
 *    the whole buffer has been sent.
 *
 *  PARAMETERS:
 *    str    Stream
 *    buf    Buffer to write to
 *    len    Length of the buffer
 *
 *  RETURNS:
 *    Number of characters written.  Can return less than 'len' if
 *    there is an error on the stream.
 *
 *  SIDE EFFECTS:
 */

int kStream_write( kStream *str, char *buf, int len);

/*-------------------------------------------------------------------
 * kStream_flush( str);
 * kStream_clear( str);
 *
 * Flush() writes any remaining data in the output buffer to the socket
 * and returns.  If the stream can't hold all of the currently buffered
 * data it will write as much as possible, and return.
 *
 * Clear() removes all data from the output-buffer without writing it
 * to the socket.
 *
 *  PARAMETERS:
 *    str    Stream
 *
 *  RETURNS:
 *    Number of characters remaining to be written, or -1 on 
 *    error.
 *
 *  SIDE EFFECTS:
 *    Removes characters from the output buffer.
 */
int kStream_flush( kStream* str);
int kStream_clear( kStream* str);

/*-------------------------------------------------------------------
 * kStream_fwrite
 *
 *    The method writes 'len' characters to the stream.
 * 
 *    This method is buffered.  If the stream can't write
 *    all of the data in one request the remaining data
 *    is saved in the output buffer and the request should 
 *    be completed by calling kStream_flush() until it
 *    returns 0.
 *
 *  PARAMETERS:
 *    str    Stream
 *    buf    Buffer to write to
 *    len    Length of the buffer
 *
 *  RETURNS:
 *    Number of characters remaining in output buffer.  Returns -1 on error.
 *
 *  SIDE EFFECTS:
 *    Adds characters to the output buffer.
 */

int kStream_fwrite( kStream *str, char *buf, int len);

/*-------------------------------------------------------------------
 * kStream_fread
 * kStream_fpeek
 *
 *    The method read 'len' characters to the stream.
 * 
 *    This method is buffered.  Data from the socket is 
 *    read and buffered until there is enough to satisfy 
 *    the request.  Any extra data will be left in the 
 *    input buffer.  If the socket runs out of data then
 *    this function will return 0, and ios->error will be set
 *    to EAGAIN.
 *
 *    Fpeek is the same as fread, but the characters read
 *    are left on the input buffer.
 *
 *  PARAMETERS:
 *    str    Stream
 *    buf    Buffer to write to
 *    len    Length of the buffer
 *
 *  RETURNS:
 *    Number of characters read.  Returns -1 on error.
 *
 *  SIDE EFFECTS:
 *    Adds characters to the input buffer.
 */
int kStream_fread( kStream *str, char *buf, int size) ;
int kStream_fpeek( kStream *str, char *buf, int size) ;

/*-------------------------------------------------------------------
 * kStream_iqlen( kStream *str);
 * kStream_oqlen( kStream *str);
 *
 *    The method returns the number of characters available in the
 *    input queue, and output queue respectively.
 *
 *  PARAMETERS:
 *    str    Stream
 *
 *  RETURNS:
 *    Number of characters available, or waiting to be written.
 *
 *  SIDE EFFECTS:
 */
int kStream_iqlen( kStream *str);
int kStream_oqlen( kStream *str);
int kStream_in_addr( kStream *str);
int kStream_out_addr( kStream *str);

#endif /* __DR1STREAM__H */
