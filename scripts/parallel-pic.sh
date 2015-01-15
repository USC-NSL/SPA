#!/bin/bash

set -e

CLIENT_BC="$1"
SERVER_BC="$2"
CLIENT_PATHS="$3"
SERVER_PATHS="$4"
VALID_PATHS="$5"
VALIDATION_SCRIPT="$6"

[ -r "$CLIENT_BC" ] || (echo "Error reading client bit-code: $CLIENT_BC."; exit 1)
[ -r "$SERVER_BC" ] || (echo "Error reading server bit-code: $SERVER_BC."; exit 1)
[ -n "$CLIENT_PATHS" ] || (echo "No client path-file specified."; exit 1)
[ -n "$SERVER_PATHS" ] || (echo "No server path-file specified."; exit 1)
[ -n "$VALID_PATHS" ] || (echo "No validated path-file specified."; exit 1)
[ -n "$VALIDATION_SCRIPT" ] || (echo "No validation script."; exit 1)

BASEFILES=""
shift 5
while [ "$1" != "" ]; dolsls 
  BASEFILES="$BASEFILES --basefile $1"
  [ -r "$1" ] || (echo "Error reading basefile: $1."; exit 1)
  shift
done

TMPDIR="`mktemp -d`"
CLIENT_PATHS_LIST="`mktemp`"
SERVER_PATHS_LIST="`mktemp`"
UNTESTED_PATHS_LIST="`mktemp`"
VALID_PATHS_LIST="`mktemp`"

spa-pic -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
  --client $CLIENT_BC --path-file $CLIENT_PATHS \
  2>&1 | tee $CLIENT_PATHS.log &
CLIENT_PID=$!

spaSplitPaths -i $CLIENT_PATHS -f -o "$TMPDIR/spa-client-%08d.paths" -p 1 &
CLIENT_SPLIT_PID=$!

(while [ 1 ]; do
  ls -1tr "$TMPDIR/spa-client-*.paths" \
    | diff -ed $CLIENT_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $CLIENT_PATHS_LIST
  sleep 1
  done) &
CLIENT_LIST_PID=$!

tail --follow=name $CLIENT_PATHS_LIST | parallel --progress --eta \
  --sshloginfile .. --controlmaster \
  --noswap -v --tag --linebuffer \
  --basefile $SERVER_BC $BASEFILES --transfer --return spa-server-{} --cleanup \
  "echo Basefiles: $BASEFILES; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
  date '+Started: %s.%N (%c)'; \
  spa-pic -max-instruction-time=10 -max-solver-time=10 -max-time=7200 \
  --path-file spa-server-{} -sender-paths {} --server $SERVER_BC; \
  date '+Finished: %s.%N (%c)';" 2>&1 | tee $SERVER_PATHS.log &
SERVER_PID=$!

(while [ 1 ]; do
  ls -1tr "$TMPDIR/spa-server-spa-client-*.paths" \
    | diff -ed $SERVER_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $SERVER_PATHS_LIST
  sleep 1
  done) &
SERVER_LIST_PID=$!

tail --follow=name $SERVER_PATHS_LIST | parallel cat {} >> $SERVER_PATHS &
SERVER_JOIN_PID=$!

spaSplitPaths -i $SERVER_PATHS -f -o "$TMPDIR/untested-%08d.paths" -p 10 &
SERVER_SPLIT_PID=$!

(while [ 1 ]; do
  ls -1tr "$TMPDIR/untested-*.paths" \
    | diff -ed $UNTESTED_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $UNTESTED_PATHS_LIST
  sleep 1
  done) &
UNTESTED_LIST_PID=$!

tail --follow=name $UNTESTED_PATHS_LIST | parallel --progress --eta \
  --sshloginfile .. --controlmaster \
  --noswap -v --tag --linebuffer \
  $BASEFILES --transfer --return valid-{} --cleanup \
  "echo Basefiles: $BASEFILES; echo Transfer: {}; echo Return: valid-{}; echo ls:; ls; \
  date '+Started: %s.%N (%c)'; \
  spa-validate {} valid-{} \"$VALIDATION_SCRIPT\"; \
  date '+Finished: %s.%N (%c)';" 2>&1 | tee $VALID_PATHS.log &
VALIDATE_PID=$!

(while [ 1 ]; do
  ls -1tr "$TMPDIR/valid-*.paths" \
    | diff -ed $VALID_PATHS_LIST - \
    | tail -n +2 | head -n -1 >> $VALID_PATHS_LIST
  sleep 1
  done) &
VALID_LIST_PID=$!

tail --follow=name $VALID_PATHS_LIST | parallel cat {} >> $VALID_PATHS &
VALID_JOIN_PID=$!

# Flush the pipeline
wait $CLIENT_PID
sleep 1
kill $CLIENT_SPLIT_PID
sleep 2
kill $CLIENT_LIST_PID
rm $CLIENT_PATHS_LIST
wait $SERVER_PID
sleep 2
kill $SERVER_LIST_PID
rm $SERVER_PATHS_LIST
wait $SERVER_JOIN_PID
sleep 1
kill $SERVER_SPLIT_PID
sleep 2
kill $UNTESTED_LIST_PID
rm $UNTESTED_PATHS_LIST
wait $VALIDATE_PID
sleep 2
kill $VALID_LIST_PID
rm $VALID_PATHS_LIST
wait $VALID_JOIN_PID

echo "Cleaning up."
rm -rf $TMPDIR

echo "Done."
