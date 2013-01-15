#include <stdio.h>

#include "openflow/openflow.h"
#include "ofpbuf.h"
#include "learning-switch.h"
#include <spa/spaRuntime.h>

struct lswitch;

void __attribute__((noinline,used)) SpaHandleQueryEntry() {
#ifdef ENABLE_SPA
   spa_message_handler_entry();
#endif
   struct lswitch *sw = lswitch_create(NULL, 1, OFP_FLOW_PERMANENT);
   struct ofpbuf buf;
   struct rconn* rc = NULL;
   buf.data = malloc(1500);
   buf.size = 1500;

   printf("sw=%p\n", sw);
   fflush(stdout);

   lswitch_process_packet(sw, rc, &buf);
}

int main(int argc, char** argv) {
   SpaHandleQueryEntry();
   return 0;
}

