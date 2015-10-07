#!/bin/sh

set -xe

# gdb --args \
spa-explore \
  --out-paths nginx-155.paths \
  --start-from SpaEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  --dump-cov-on-interrupt "nginx-155.cov" \
  --use-shallow-distance \
  out/llvm/objs/nginx \
  2>&1 | tee nginx-155.paths.log
#   --step-debug \
#   --dump-cfg cfg \
#   out-1.5.5/llvm/objs/nginx \

# spa-explore \
#   --in-paths nginx-155.paths \
#   --connect spa_in_msg_message=spa_out_msg_response \
#   --connect spa_in_msg_size_message=spa_out_msg_size_response \
#   --out-paths nginx-155-status.paths \
#   --start-from Entry \
#   --output-at spa_msg_output_point \
#   ../spa-tools/spdy-response-status.bc
# 
# spa-explore \
#   --in-paths nginx-155.paths --connect-in-in \
#   --out-paths nginx-174.paths \
#   --start-from SpaEntry \
#   --toward "spa_msg_output_point" \
#   --stop-at "spa_msg_output_point" \
#   --output-at "spa_msg_output_point" \
#   out-1.7.4/llvm/objs/nginx
# 
# spa-explore \
#   --in-paths nginx-174.paths \
#   --connect spa_in_msg_message1=spa_out_msg_response \
#   --connect spa_in_msg_size_message1=spa_out_msg_size_response \
#   --connect spa_in_msg_message2=spa_out_msg_response2 \
#   --connect spa_in_msg_size_message2=spa_out_msg_size_response2 \
#   --out-paths server-equiv.paths \
#   --start-from MsgCompareEntry \
#   --output-at MsgsDifferent \
#   --max-instruction-time=5 \
#   ../spa-tools/msg-compare.bc
