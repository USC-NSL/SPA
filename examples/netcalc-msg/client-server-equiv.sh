#!/bin/sh

set -xe

spa-explore \
  --out-paths netcalc-client.paths \
  --start-from SpaExecuteQueryEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  netcalc-client.bc

spa-explore \
  --in-paths netcalc-client.paths --connect-in-out \
  --out-paths netcalc-server.paths \
  --start-from SpaHandleQueryEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  netcalc-server.bc

spa-explore \
  --in-paths netcalc-server.paths --connect-in-in \
  --out-paths netcalc-server-fixed.paths \
  --start-from SpaHandleQueryEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  netcalc-server-fixed.bc

spa-explore \
  --in-paths netcalc-server-fixed.paths \
  --connect spa_in_msg_message1=spa_out_msg_response \
  --connect spa_in_msg_size_message1=spa_out_msg_size_response \
  --connect spa_in_msg_message2=spa_out_msg_response2 \
  --connect spa_in_msg_size_message2=spa_out_msg_size_response2 \
  --out-paths client-server-equiv.paths \
  --start-from MsgCompareEntry \
  --output-at MsgsDifferent \
  --max-instruction-time=5 \
  ../spa-tools/msg-compare.bc
