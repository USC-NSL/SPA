#include <stdio.h>

#include "openflow/openflow.h"
#include "vconn.h"
#include "ofpbuf.h"


int main(int argc, char** argv) {
   char crap[1000];
   if (is_admitted_msg((struct ofpbuf*)crap)) {
      printf("yup\n");
   }
   else {
      printf("nope\n");
   }
   return 0;
}

