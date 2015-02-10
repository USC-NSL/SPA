#!/bin/bash

set -e

CLIENT_BC="$1"
SERVER_BC="$2"
WORK_DIR="$3"
CLIENT_INVOKE="$4"
SERVER_INVOKE="$5"
PORT="$6"
CLIENT_PATHS="$7"
SERVER_PATHS="$8"
VALID_PATHS="$9"

[ -n "$CLIENT_BC" ] || (echo "No client bit-code."; exit 1)
[ -n "$SERVER_BC" ] || (echo "No server bit-code."; exit 1)
[ -n "$WORK_DIR" ] || (echo "No working directory."; exit 1)
[ -n "$CLIENT_INVOKE" ] || (echo "No client invocation specified."; exit 1)
[ -n "$SERVER_INVOKE" ] || (echo "No server invocation specified."; exit 1)
[ -n "$PORT" ] || (echo "No port specified."; exit 1)
[ -n "$CLIENT_PATHS" ] || (echo "No client path-file specified."; exit 1)
[ -n "$SERVER_PATHS" ] || (echo "No server path-file specified."; exit 1)
[ -n "$VALID_PATHS" ] || (echo "No validated path-file specified."; exit 1)

TMPDIR="`mktemp -d`"
CLIENT_PATHS_LIST="$TMPDIR/client-paths.list"
SERVER_PATHS_LIST="$TMPDIR/server-paths.list"
UNTESTED_PATHS_LIST="$TMPDIR/untested-paths.list"
VALID_PATHS_LIST="$TMPDIR/valid-paths.list"

echo "Working in $TMPDIR."

rm -f $CLIENT_PATHS $SERVER_PATHS $VALID_PATHS
touch $CLIENT_PATHS $SERVER_PATHS $VALID_PATHS

# Set up pipeline
mkfifo $CLIENT_PATHS_LIST
inotifywait -m -e CLOSE_WRITE --format '%w%f' $TMPDIR \
  > $CLIENT_PATHS_LIST &
CLIENT_LIST_PID=$!

mkfifo $SERVER_PATHS_LIST
inotifywait -q -m -e MOVED_TO --format '%w%f' $TMPDIR \
  > $SERVER_PATHS_LIST &
SERVER_LIST_PID=$!

mkfifo $UNTESTED_PATHS_LIST
inotifywait -q -m -e CLOSE_WRITE --format '%w%f' $TMPDIR \
  > $UNTESTED_PATHS_LIST &
UNTESTED_LIST_PID=$!

mkfifo $VALID_PATHS_LIST
inotifywait -q -m -e MOVED_TO --format '%w%f' $TMPDIR \
  > $VALID_PATHS_LIST &
VALID_LIST_PID=$!

spaSplitPaths -i $CLIENT_PATHS -f -o "$TMPDIR/%08d-client.paths" -p 1 &
CLIENT_SPLIT_PID=$!

grep --line-buffered '/[0-9]*-client.paths$' $CLIENT_PATHS_LIST \
  | parallel --progress \
    --sshloginfile .. \
    --noswap --load 95% \
    -v --tag --linebuffer \
    --transfer --return "{.}-server.paths" --cleanup \
    "cd $WORK_DIR; \
    echo Transfer: {}; echo Return: {.}-server.paths; echo pwd: \$PWD; \
    echo ls:; ls; \
    date '+Started: %s.%N (%c)'; \
    \$HOME/spa/Release+Asserts/bin/spa-pic \
      -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
      --path-file {.}-server.paths -sender-paths {} --server $SERVER_BC; \
    date '+Finished: %s.%N (%c)';" >$SERVER_PATHS.log 2>&1 &
SERVER_PID=$!

grep --line-buffered '/[0-9]*-client-server.paths$' $SERVER_PATHS_LIST \
  | parallel cat {} > $SERVER_PATHS &
SERVER_JOIN_PID=$!

spaSplitPaths -i $SERVER_PATHS -f -o "$TMPDIR/%08d-untested.paths" -p 1 &
SERVER_SPLIT_PID=$!

grep --line-buffered '/[0-9]*-untested.paths$' $UNTESTED_PATHS_LIST \
  | parallel --progress \
    --sshloginfile .. \
    --noswap \
    -v --tag --linebuffer \
    --transfer --return "{.}-valid.paths" --cleanup \
    "cd $WORK_DIR; \
    echo Transfer: {}; echo Return: {.}-valid.paths; echo pwd: \$PWD; \
    echo ls:; ls; \
    echo netns: spa{#}; \
    echo user: \$USER; \
    date '+Started: %s.%N (%c)'; \
    sudo ip netns add spa{#}; \
    sudo ip netns exec spa{#} ifconfig lo 127.0.0.1/8 up; \
    sudo ip netns exec spa{#} sudo -u \$USER \
      \$HOME/spa/Release+Asserts/bin/spaE2ETest \
        {} {.}-valid.paths /dev/null \
        $PORT \"$CLIENT_INVOKE\" \"$SERVER_INVOKE\"; \
    sudo ip netns delete spa{#}; \
    date '+Finished: %s.%N (%c)';" >$VALID_PATHS.log 2>&1 &
VALIDATE_PID=$!

grep --line-buffered '/[0-9]*-untested-valid.paths$' $VALID_PATHS_LIST \
  | parallel cat {} > $VALID_PATHS &
VALID_JOIN_PID=$!

spa-cluster-manual -f $VALID_PATHS &
CLUSTER_PID=$!

sleep 1

# Start running.
(START_TIME="`date '+%s.%N'`"
while [ 1 ]; do
  CUR_TIME="`date '+%s.%N'`"
  echo "[`echo $CUR_TIME - $START_TIME | bc -l | sed 's/^\./0./'`]" \
       "Produced" \
       `grep -e '--- PATH START ---' $CLIENT_PATHS 2>/dev/null | wc -l` \
       "client paths," \
       `grep -e '--- PATH START ---' $SERVER_PATHS 2>/dev/null | wc -l` \
       "server paths, and" \
       `grep -e '--- PATH START ---' $VALID_PATHS 2>/dev/null | wc -l` \
       "valid path."
  sleep 1
done) &
STATS_PID=$!

spa-pic -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
  --client $CLIENT_BC --path-file $CLIENT_PATHS \
  >$CLIENT_PATHS.log 2>&1 &
CLIENT_PID=$!

function interrupt {
  echo "Killing client."
  while kill $CLIENT_PID; do sleep 1; done
  echo "Killing client split."
  while kill $CLIENT_SPLIT_PID; do sleep 1; done
  echo "Killing client list."
  while kill $CLIENT_LIST_PID; do sleep 1; done
  echo "Killing server."
  while kill $SERVER_PID; do sleep 1; done
  echo "Killing server list."
  while kill $SERVER_LIST_PID; do sleep 1; done
  echo "Killing server join."
  while kill $SERVER_JOIN_PID; do sleep 1; done
  echo "Killing server split."
  while kill $SERVER_SPLIT_PID; do sleep 1; done
  echo "Killing untested list."
  while kill $UNTESTED_LIST_PID; do sleep 1; done
  echo "Killing validate."
  while kill $VALIDATE_PID; do sleep 1; done
  echo "Killing valid list."
  while kill $VALID_LIST_PID; do sleep 1; done
  echo "Killing valid join."
  while kill $VALID_JOIN_PID; do sleep 1; done
  echo "Killing cluster."
  while kill $CLUSTER_PID; do sleep 1; done
  echo "Killing stats."
  while kill $STATS_PID; do sleep 1; done

  echo "Waiting."
  wait

  echo "Cleaning up."
  rm -rf $TMPDIR

  echo "Done."
}
trap interrupt SIGINT SIGTERM

# Flush the pipeline
wait $CLIENT_PID || true
echo "Client analysis complete."
sleep 1
kill $CLIENT_SPLIT_PID
wait $CLIENT_SPLIT_PID 2>/dev/null || true
kill $CLIENT_LIST_PID
wait $CLIENT_LIST_PID 2>/dev/null || true
wait $SERVER_PID || true
echo "Server analysis complete."
kill $SERVER_LIST_PID
wait $SERVER_LIST_PID 2>/dev/null || true
wait $SERVER_JOIN_PID || true
sleep 1
kill $SERVER_SPLIT_PID
wait $SERVER_SPLIT_PID 2>/dev/null || true
kill $UNTESTED_LIST_PID
wait $UNTESTED_LIST_PID 2>/dev/null || true
wait $VALIDATE_PID || true
echo "Validation analysis complete."
kill $VALID_LIST_PID
wait $VALID_LIST_PID 2>/dev/null || true
wait $VALID_JOIN_PID || true
kill $CLUSTER_PID
wait $CLUSTER_PID 2>/dev/null || true
sleep 1
kill $STATS_PID
wait $STATS_PID 2>/dev/null || true

echo "Cleaning up."
rm -rf $TMPDIR

echo "Done."
