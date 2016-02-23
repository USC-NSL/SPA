#!/bin/bash

set -e

distributed-conversational-analysis.sh kv.paths \
  "--shallow-exploration --connect-sockets --start-from spa_entry  --toward kv_done                --output-at spa_msg_no_input_point --output-at kv_done --participant kv-client   --ip 127.0.0.1 spa/examples/kv/kv-client-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry1 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-1 --ip 127.0.0.2 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry2 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-2 --ip 127.0.0.3 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry3 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-3 --ip 127.0.0.4 spa/examples/kv/kv-server-udp.bc" &

sleep 1

echo "Launching conversation exploration."
spa-explore-conversation \
    --connect-sockets \
    --follow-in-paths \
    kv.paths >/dev/null 2>&1 &

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

wait
