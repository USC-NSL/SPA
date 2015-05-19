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
  --in-paths netcalc-client.paths \
  --connect "spa_in_msg_in_query=spa_out_msg_query" \
  --connect "spa_in_msg_size_in_query=spa_out_msg_size_query" \
  --out-paths netcalc-proxy.paths \
  --start-from SpaHandleQueryEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  netcalc-proxy.bc

spa-explore \
  --in-paths netcalc-proxy.paths \
  --connect "spa_in_msg_query=spa_out_msg_out_query" \
  --connect "spa_in_msg_size_query=spa_out_msg_size_out_query" \
  --out-paths client-proxy-server-interop.paths \
  --start-from SpaHandleQueryEntry \
  --away-from spa_valid_path_point \
  --output-terminal \
  --output-at "NOT REACHING spa_valid_path_point" \
  --filter-output "REACHED spa_msg_input_point \
                   AND NOT REACHED spa_valid_path_point" \
  netcalc-server.bc
