#!/bin/bash

set -e

[ -r "$1" ] || (echo "Error reading bit-code: $1."; exit 1)
[ -r "$2" ] || (echo "Error reading client paths: $2."; exit 1)
[ -n "$3" ] || (echo "No server path-file specied."; exit 1)

rm -f spa-client-*.paths spa-server-*.paths $3.joblog

spaSplitPaths -i $2 -o 'spa-client-%04d.paths' -n 1000

parallel --progress --joblog $3.joblog \
  --controlmaster -v --tag --linebuffer \
  -S lpedrosa@spa1.pedrosa.3-a.net -S lpedrosa@spa2.pedrosa.3-a.net -S lpedrosa@spa3.pedrosa.3-a.net \
  --basefile $1 --transfer --return spa-server-{} --cleanup \
  "echo Basefile: $1; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
  date '+Started: %s.%N (%c)'; \
  /home/lpedrosa/spa/Release+Asserts/bin/spa --path-file spa-server-{} -sender-paths {} --server $1; \
  date '+Finished: %s.%N (%c)';" \
  ::: spa-client-*.paths 2>&1 | tee $3.log

# parallel --progress --joblog $3.joblog \
#   --controlmaster -v --tag --linebuffer \
#   -S david@192.168.3.6 \
#   --basefile $1 --transfer --return spa-server-{} --cleanup \
#   "echo Basefile: $1; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
#   date '+Started: %s.%N (%c)'; \
#   /home/lpedrosa/spa/Release+Asserts/bin/spa --path-file spa-server-{} -sender-paths {} --server $1; \
#   date '+Finished: %s.%N (%c)';" \
#   ::: spa-client-*.paths 2>&1 | tee $3.log

cat spa-server-*.paths > $3

rm -f spa-client-*.paths spa-server-*.paths $3.joblog
