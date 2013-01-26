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
   struct lswitch *sw = spa_switch_create();
   struct ofpbuf buf;
   struct rconn* rc = NULL;
   buf.data = malloc(1500);
   buf.size = 1500;

   lswitch_process_packet(sw, rc, &buf);
}

int main(int argc, char** argv) {
   SpaHandleQueryEntry();
   return 0;
}


