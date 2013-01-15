#include <stdio.h>

#include "openflow/openflow.h"
#include "ofpbuf.h"
#include "datapath.h"
#include <spa/spaRuntime.h>

struct lswitch;

void __attribute__((noinline,used)) SpaHandleQueryEntry() {
#ifdef ENABLE_SPA
   spa_api_entry();
#endif
   struct ofpbuf buf;
   buf.data = (void*)(((char*)malloc(3000)) + 1500);
   buf.size = 1500;

   dp_output_control(NULL, &buf, 0, OFP_DEFAULT_MISS_SEND_LEN, OFPR_NO_MATCH);
}
/*
int main(int argc, char** argv) {
   SpaHandleQueryEntry();
   return 0;
}
*/

