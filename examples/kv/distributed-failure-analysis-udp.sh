#!/bin/bash

set -e

TARGET_CONVERSATION="kv-client kv-server-1 kv-server-2 kv-server-3 kv-client"
FAULTMODEL="symbolic"

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

distributed-conversational-analysis.sh kv.paths \
  "--shallow-exploration --connect-sockets --start-from spa_entry  --fault-model $FAULTMODEL --toward kv_done                --output-at spa_msg_no_input_point --output-at kv_done --participant kv-client   --ip 127.0.0.1 spa/examples/kv/kv-client-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry1 --fault-model $FAULTMODEL --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-1 --ip 127.0.0.2 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry2 --fault-model $FAULTMODEL --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-2 --ip 127.0.0.3 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration --connect-sockets --start-from spa_entry3 --fault-model $FAULTMODEL --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-3 --ip 127.0.0.4 spa/examples/kv/kv-server-udp.bc" \
   "$TARGET_CONVERSATION" &
SPA_PID=$!

sleep 1

echo "Launching conversation exploration."
spa-explore-conversation \
    --connect-sockets \
    --follow-in-paths \
    kv.paths \
    >>kv.paths.log 2>&1 &

echo "Follow analysis progress on: http://localhost:8000/"
spa-doc --serve-http 8000 \
    --map-src /home/lpedrosa/spa=/home/david/Projects/spa \
    --color-filter "lightgreen:REACHED kv_done" \
    --color-filter "cyan:CONVERSATION $TARGET_CONVERSATION" \
    kv.paths &

wait $SPA_PID
echo "Analysis is complete. Ctrl-C when ready."
wait
