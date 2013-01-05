#include <stdio.h>

#include "openflow/openflow.h"
#include "vconn.h"
#include "ofpbuf.h"

static bool
is_admitted_msg(const struct ofpbuf *b)
{
    struct ofp_header *oh = b->data;
    uint8_t type = oh->type;
    return !(type < 32                                                                                                                      
             && (1u << type) & ((1u << OFPT_HELLO) |
                                (1u << OFPT_ERROR) |
                                (1u << OFPT_ECHO_REQUEST) |
                                (1u << OFPT_ECHO_REPLY) |
                                (1u << OFPT_VENDOR) |
                                (1u << OFPT_FEATURES_REQUEST) |
                                (1u << OFPT_FEATURES_REPLY) |
                                (1u << OFPT_GET_CONFIG_REQUEST) |
                                (1u << OFPT_GET_CONFIG_REPLY) |
                                (1u << OFPT_SET_CONFIG)));
}

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

