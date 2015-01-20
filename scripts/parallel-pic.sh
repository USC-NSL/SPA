#!/bin/bash

set -e

CLIENT_BC="$1"
SERVER_BC="$2"
CLIENT_PATHS="$3"
SERVER_PATHS="$4"
VALID_PATHS="$5"
VALIDATION_SCRIPT="$6"

[ -r "$CLIENT_BC" ] \
  || (echo "Error reading client bit-code: $CLIENT_BC."; exit 1)
[ -r "$SERVER_BC" ] \
  || (echo "Error reading server bit-code: $SERVER_BC."; exit 1)
[ -n "$CLIENT_PATHS" ] || (echo "No client path-file specified."; exit 1)
[ -n "$SERVER_PATHS" ] || (echo "No server path-file specified."; exit 1)
[ -n "$VALID_PATHS" ] || (echo "No validated path-file specified."; exit 1)
[ -n "$VALIDATION_SCRIPT" ] || (echo "No validation script."; exit 1)

BASEFILES=""
shift 6
while [ "$1" != "" ]; do
  BASEFILES="$BASEFILES --basefile $1"
  [ -r "$1" ] || (echo "Error reading basefile: $1."; exit 1)
  shift
done

TMPDIR="`mktemp -d`"
CLIENT_PATHS_LIST="$TMPDIR/client-paths.list"
SERVER_PATHS_LIST="$TMPDIR/server-paths.list"
UNTESTED_PATHS_LIST="$TMPDIR/untested-paths.list"
VALID_PATHS_LIST="$TMPDIR/valid-paths.list"

echo "Working in $TMPDIR."

rm -f $CLIENT_PATHS $SERVER_PATHS $VALID_PATHS
touch $CLIENT_PATHS $SERVER_PATHS $VALID_PATHS

spa-pic -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
  --client $CLIENT_BC --path-file $CLIENT_PATHS \
  >$CLIENT_PATHS.log 2>&1 &
CLIENT_PID=$!

spaSplitPaths -i $CLIENT_PATHS -f -o "$TMPDIR/%08d-client.paths" -p 1 &
CLIENT_SPLIT_PID=$!

touch $CLIENT_PATHS_LIST
(while [ 1 ]; do
  kill -0 $CLIENT_SPLIT_PID 2>/dev/null || true $((DONE=1))
  ls -1tr $(eval echo '$TMPDIR/*-client.paths') 2>/dev/null \
    | diff -ed $CLIENT_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $CLIENT_PATHS_LIST
  if [ "$DONE" ]; then
    echo "Finished collecting client paths."
    sleep 1
    rm $CLIENT_PATHS_LIST
    exit 0
  fi
  sleep 1
done) &
CLIENT_LIST_PID=$!

tail --follow=name $CLIENT_PATHS_LIST 2>/dev/null \
  | parallel --progress --eta \
      --sshloginfile .. --controlmaster \
      --noswap -v --tag --linebuffer \
      --basefile $SERVER_BC $BASEFILES \
      --transfer --return "{.}-server.paths" --cleanup \
      "echo Basefiles: $BASEFILES; \
       echo Transfer: {}; echo Return: {.}-server.paths; echo ls:; ls; \
       date '+Started: %s.%N (%c)'; \
       spa/Release+Asserts/bin/spa-pic \
         -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
         --path-file {.}-server.paths -sender-paths {} --server $SERVER_BC; \
       date '+Finished: %s.%N (%c)';" >$SERVER_PATHS.log 2>&1 &
SERVER_PID=$!

touch $SERVER_PATHS_LIST
(while [ 1 ]; do
  kill -0 $SERVER_PID 2>/dev/null || true $((DONE=1))
  ls -1tr $(eval echo '$TMPDIR/*-client-server.paths') 2>/dev/null \
    | diff -ed $SERVER_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $SERVER_PATHS_LIST
  if [ "$DONE" ]; then
    echo "Finished collecting server paths."
    sleep 1
    rm $SERVER_PATHS_LIST
    exit 0
  fi
  sleep 1
done) &
SERVER_LIST_PID=$!

tail --follow=name $SERVER_PATHS_LIST 2>/dev/null \
  | parallel cat {} >> $SERVER_PATHS &
SERVER_JOIN_PID=$!

spaSplitPaths -i $SERVER_PATHS -f -o "$TMPDIR/%08d-untested.paths" -p 10 &
SERVER_SPLIT_PID=$!

touch $UNTESTED_PATHS_LIST
(while [ 1 ]; do
  kill -0 $SERVER_SPLIT_PID 2>/dev/null || true $((DONE=1))
  ls -1tr $(eval echo '$TMPDIR/*-untested.paths') 2>/dev/null \
    | diff -ed $UNTESTED_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $UNTESTED_PATHS_LIST
  if [ "$DONE" ]; then
    echo "Finished collecting untested paths."
    sleep 1
    rm $UNTESTED_PATHS_LIST
    exit 0
  fi
  sleep 1
  done) &
UNTESTED_LIST_PID=$!

# tail --follow=name $UNTESTED_PATHS_LIST 2>/dev/null \
#   | parallel --progress --eta \
#       --sshloginfile .. --controlmaster \
#       --noswap -v --tag --linebuffer \
#       $BASEFILES --transfer --return "{.}-valid.paths" --cleanup \
#       "echo Basefiles: $BASEFILES; \
#       echo Transfer: {}; echo Return: {.}-valid.paths; echo ls:; ls; \
#       echo netns: spa{#}; \
#       echo user: \$USER; \
#       date '+Started: %s.%N (%c)'; \
#       sudo ip netns add spa{#}; \
#       sudo ip netns exec spa{#} ifconfig lo 127.0.0.1/8 up; \
#       sudo ip netns exec spa{#} sudo -u \$USER \
#         LD_LIBRARY_PATH=spa/Release+Asserts/lib \
#           spa/Release+Asserts/bin/spa-validate \
#             -d -gcno-dir-map $PWD=. \
#             {} {.}-valid.paths \
#             \"$VALIDATION_SCRIPT\"; \
#       sudo ip netns delete spa{#}; \
#       date '+Finished: %s.%N (%c)';" 2>&1 | tee $VALID_PATHS.log &
tail --follow=name $UNTESTED_PATHS_LIST 2>/dev/null \
  | parallel -j 1 --progress --eta \
      --noswap -v --tag --linebuffer \
      "spa-validate {} {.}-valid.paths.tmp \"$VALIDATION_SCRIPT\"; \
       mv {.}-valid.paths.tmp {.}-valid.paths" \
      >$VALID_PATHS.log 2>&1 &
VALIDATE_PID=$!

touch $VALID_PATHS_LIST
(while [ 1 ]; do
  kill -0 $VALIDATE_PID 2>/dev/null || true $((DONE=1))
  ls -1tr $(eval echo '$TMPDIR/*-valid.paths') 2>/dev/null \
    | diff -ed $VALID_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $VALID_PATHS_LIST
  if [ "$DONE" ]; then
    echo "Finished collecting valid paths."
    sleep 1
    rm $VALID_PATHS_LIST
    exit 0
  fi
  sleep 1
  done) &
VALID_LIST_PID=$!

tail --follow=name $VALID_PATHS_LIST 2>/dev/null \
  | parallel cat {} >> $VALID_PATHS &
VALID_JOIN_PID=$!

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

# Flush the pipeline
wait $CLIENT_PID
sleep 1
kill $CLIENT_SPLIT_PID
wait $CLIENT_SPLIT_PID 2>/dev/null || true
wait $CLIENT_LIST_PID
wait $SERVER_PID
wait $SERVER_LIST_PID
wait $SERVER_JOIN_PID
sleep 1
kill $SERVER_SPLIT_PID
wait $SERVER_SPLIT_PID 2>/dev/null || true
wait $UNTESTED_LIST_PID
wait $VALIDATE_PID
wait $VALID_LIST_PID
wait $VALID_JOIN_PID
sleep 1
kill $STATS_PID
wait $STATS_PID 2>/dev/null || true

echo "Cleaning up."
rm -rf $TMPDIR

echo "Done."
