#!/bin/bash

set -e

parallel-conversational-analysis.sh kv.paths \
  "--connect-sockets --start-from spa_entry --toward kv_done --output-at spa_msg_no_input_point --output-at kv_done --participant kv-client spa/examples/kv/kv-client-tcp.bc; \
   --connect-sockets --start-from spa_entry1 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-1 spa/examples/kv/kv-server-tcp.bc; \
   --connect-sockets --start-from spa_entry2 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-2 spa/examples/kv/kv-server-tcp.bc; \
   --connect-sockets --start-from spa_entry3 --toward spa_msg_no_input_point --output-at spa_msg_no_input_point --participant kv-server-3 spa/examples/kv/kv-server-tcp.bc"

trap 'kill $(jobs -p)' EXIT

wait
