#!/bin/bash

rm -f spa-client-*.paths spa-server-*.paths $3.joblog

spaSplitPaths -i $2 -o 'spa-client-%03d.paths' -n 500

parallel --progress --joblog $3.joblog \
  --controlmaster -v --tag --ungroup --linebuffer \
  -S lpedrosa@spa1.pedrosa.3-a.net -S lpedrosa@spa2.pedrosa.3-a.net -S lpedrosa@spa3.pedrosa.3-a.net \
  --basefile $1 --transfer --return spa-server-{} --cleanup \
  "echo Basefile: $1; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
  /home/lpedrosa/spa/Release+Asserts/bin/spa --path-file spa-server-{} -sender-paths {} --server $1" \
  ::: spa-client-*.paths 2>&1 | tee $3.log

# parallel --progress --joblog $3.joblog \
#   --controlmaster -v --tag --ungroup --linebuffer \
#   -S david@192.168.3.6 \
#   --basefile $1 --transfer --return spa-server-{} --cleanup \
#   "echo Basefile: $1; echo Transfer: {}; echo Return: spa-server-{}; echo ls:; ls; \
#   /home/david/Projects/spa/Release+Asserts/bin/spa --path-file spa-server-{} -sender-paths {} --server $1" \
#   ::: spa-client-*.paths 2>&1 | tee $3.log

cat spa-server-*.paths > $3

rm -f spa-client-*.paths spa-server-*.paths $3.joblog
