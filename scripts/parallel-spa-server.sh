#!/bin/bash

set -e

[ -r "$1" ] || (echo "Error reading client paths: $1."; exit 1)
[ -n "$2" ] || (echo "No server path-file specified."; exit 1)
[ -r "$3" ] || (echo "Error reading server bit-code: $3."; exit 1)

CLIENT_PATHS=$1
SERVER_PATHS=$2
SERVER_BC=$3

shift 2
while [ "$1" != "" ]; do
	BASEFILES="$BASEFILES --basefile $1"
	[ -r "$1" ] || (echo "Error reading basefile: $3."; exit 1)
	shift
done

rm -f spa-client-*.paths spa-server-*.paths $SERVER_PATHS.joblog

spaSplitPaths -i $CLIENT_PATHS -o 'spa-client-%04d.paths' -p 1

parallel --progress --joblog $SERVER_PATHS.joblog \
  --controlmaster -v --tag --linebuffer \
  -S lpedrosa@spa1.pedrosa.3-a.net -S lpedrosa@spa2.pedrosa.3-a.net -S lpedrosa@spa3.pedrosa.3-a.net \
  $BASEFILES --transfer --return spa-server-{} --cleanup \
  "echo Basefiles: $BASEFILES; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
  date '+Started: %s.%N (%c)'; \
  /home/lpedrosa/spa/Release+Asserts/bin/spa -max-instruction-time=1 -max-solver-time=1 --path-file spa-server-{} -sender-paths {} --server $SERVER_BC; \
  date '+Finished: %s.%N (%c)';" \
  ::: spa-client-*.paths 2>&1 | tee $SERVER_PATHS.log

# parallel --progress --joblog $SERVER_PATHS.joblog \
#   --controlmaster -v --tag --linebuffer \
#   -S david@192.168.3.6 \
#   $BASEFILES --transfer --return spa-server-{} --cleanup \
#   "echo Basefiles: $BASEFILES; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
#   date '+Started: %s.%N (%c)'; \
#   /home/lpedrosa/spa/Release+Asserts/bin/spa -max-instruction-time=1 -max-solver-time=1 --path-file spa-server-{} -sender-paths {} --server $SERVER_BC; \
#   date '+Finished: %s.%N (%c)';" \
#   ::: spa-client-*.paths 2>&1 | tee $SERVER_PATHS.log

cat spa-server-*.paths > $SERVER_PATHS

rm -f spa-client-*.paths spa-server-*.paths $SERVER_PATHS.joblog
