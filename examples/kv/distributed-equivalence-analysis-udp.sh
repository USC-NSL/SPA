#!/bin/bash

set -e

TARGET_CONVERSATION="kv-local kv-client kv-server-1 kv-server-2 kv-server-3 kv-client kv-server-1 kv-server-2 kv-server-3 kv-client compare"

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

distributed-conversational-analysis.sh kv.paths \
  "--shallow-exploration                                                                                  --start-from spa_setget --toward kv_done                                                   --output-at kv_done --participant kv-local    --ip 127.0.0.0 spa/examples/kv/kv-local.bc; \
   --shallow-exploration --connect spa_in_api_value_kv-client=spa_in_api_value_kv-local --connect-sockets --start-from spa_setget --toward kv_done                --output-at spa_msg_no_input_point --output-at kv_done --participant kv-client   --ip 127.0.0.1 spa/examples/kv/kv-client-udp.bc; \
   --shallow-exploration                                                                --connect-sockets --start-from spa_entry1 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-1 --ip 127.0.0.2 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration                                                                --connect-sockets --start-from spa_entry2 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-2 --ip 127.0.0.3 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration                                                                --connect-sockets --start-from spa_entry3 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point                     --participant kv-server-3 --ip 127.0.0.4 spa/examples/kv/kv-server-udp.bc; \
   --shallow-exploration --connect spa_in_api_in1_compare=spa_out_api_value_kv-local --connect spa_in_api_size_in1_compare=spa_out_api_size_value_kv-local --connect spa_in_api_in2_compare=spa_out_api_value_kv-client --connect spa_in_api_size_in2_compare=spa_out_api_size_value_kv-client --start-from ApiCompareEntry --toward done --output-at done --participant compare --ip 127.0.0.5 spa/examples/spa-tools/msg-compare.bc" \
   "$TARGET_CONVERSATION" &
SPA_PID=$!

sleep 1

echo "Launching conversation exploration."
spa-explore-conversation \
    --connect-sockets \
    --connect spa_in_api_value_kv-client=spa_in_api_value_kv-local \
    --connect spa_in_api_in1_compare=spa_out_api_value_kv-local \
    --connect spa_in_api_size_in1_compare=spa_out_api_size_value_kv-local \
    --connect spa_in_api_in2_compare=spa_out_api_value_kv-client \
    --connect spa_in_api_size_in2_compare=spa_out_api_size_value_kv-client \
    --follow-in-paths \
    kv.paths \
    >>kv.paths.log 2>&1 &

echo "Follow analysis progress on: http://localhost:8000/"
spa-doc --serve-http 8000 \
    --map-src /home/lpedrosa/spa=/home/david/Projects/spa \
    --color-filter "lightgreen:REACHED Equal" \
    --color-filter "orangered:REACHED Different" \
    --color-filter "cyan:CONVERSATION $TARGET_CONVERSATION" \
    kv.paths &

wait $SPA_PID
echo "Analysis is complete. Ctrl-C when ready."
wait
