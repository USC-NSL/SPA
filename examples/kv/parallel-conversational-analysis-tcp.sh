#!/bin/bash

set -e

parallel-conversational-analysis.sh kv.paths \
  "--shallow-exploration --connect-sockets --start-from spa_entry --toward kv_done --output-at spa_msg_no_input_point --output-at kv_done --participant kv-client --ip 127.0.0.1 spa/examples/kv/kv-client-tcp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry1 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-1   --ip 127.0.0.2 spa/examples/kv/kv-server-tcp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry2 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-2   --ip 127.0.0.3 spa/examples/kv/kv-server-tcp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry3 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-3   --ip 127.0.0.4 spa/examples/kv/kv-server-tcp.bc" &

sleep 1

spa-explore-conversation \
    --connect-sockets \
    --participant-ip kv-client:127.0.0.1 \
    --participant-ip kv-server-1:127.0.0.2 \
    --participant-ip kv-server-2:127.0.0.3 \
    --participant-ip kv-server-3:127.0.0.4 \
    --follow-in-paths \
    kv.paths \
    2>&1 | tee derived.log &

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

wait
