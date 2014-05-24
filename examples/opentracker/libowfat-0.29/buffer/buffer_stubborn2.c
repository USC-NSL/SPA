#include <errno.h>
#include "buffer.h"

int buffer_stubborn_read(ssize_t (*op)(),int fd,const char* buf, size_t len,void* cookie) {
  int w;
  for (;;) {
    if ((w=op(fd,buf,len,cookie))<0)
      if (errno == EINTR) continue;
    break;
  }
  return w;
}

