#!/bin/bash

set -e

[ -r "$1" ] || (echo "Error reading server paths: $2."; exit 1)
[ -n "$2" ] || (echo "No output specified."; exit 1)

rm -f badinputs-*.paths badinputs-*.txt $2.joblog

spaSplitPaths -i $1 -o 'badinputs-%04d.paths' -p 10

parallel --progress --joblog $2.joblog \
  --controlmaster -v --tag --linebuffer \
  -S lpedrosa@spa1.pedrosa.3-a.net -S lpedrosa@spa2.pedrosa.3-a.net -S lpedrosa@spa3.pedrosa.3-a.net \
  --transfer --return badinputs-{}.txt --cleanup \
  "echo Transfer: {}; echo Return: badinputs-{}.txt; echo ls:; ls; \
  date '+Started: %s.%N (%c)'; \
  /home/lpedrosa/spa/Release+Asserts/bin/spaBadInputs --server {} -o badinputs-{}.txt -d /dev/null -p 1 -j 10 -w 1; \
  date '+Finished: %s.%N (%c)';" \
  ::: badinputs-*.paths 2>&1 | tee $2.log

# parallel --progress --joblog $2.joblog \
#   --controlmaster -v --tag --linebuffer \
#   -S david@192.168.3.6 \
#   --transfer --return badinputs-{}.txt --cleanup \
#   "echo Transfer: {}; echo Return: badinputs-{}.txt; echo ls:; ls; \
#   date '+Started: %s.%N (%c)'; \
#   /home/lpedrosa/spa/Release+Asserts/bin/spaBadInputs --server {} -o badinputs-{}.txt -d /dev/null -p 1 -j 10 -w 1; \
#   date '+Finished: %s.%N (%c)';" \
#   ::: badinputs-*.paths 2>&1 | tee $2.log

cat badinputs-*.txt > $2

rm -f badinputs-*.paths badinputs-*.txt $2.joblog
