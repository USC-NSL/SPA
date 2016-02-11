#!/bin/bash

set -e

PATH_FILE="$1"
IFS=';' read -ra OPTS <<< "$2"

echo "Starting conversational analysis with $(echo ${!OPTS[@]} | wc -w) participants."
echo "Conversation recorded to $PATH_FILE."

for i in ${!OPTS[@]}; do
  echo "Participant $i analyzed with equivalent of:"
  echo "  spa-explore \\"
  echo "      --in-paths $PATH_FILE \\"
  echo "      --follow-in-paths \\"
  echo "      --out-paths $PATH_FILE \\"
  echo "      --out-paths-append \\"
  echo "      ${OPTS[i]}"
done

[ "$PATH_FILE" ] || PATH_FILE="conversation.paths"

# TMPDIR="."
TMPDIR="`mktemp -d`"
echo "Working in $TMPDIR"

rm -f $PATH_FILE
touch $PATH_FILE

# Set up pipeline
for i in ${!OPTS[@]}; do
  rm -f participant-$i.log

  spaSplitPaths -i $PATH_FILE -f -o "$TMPDIR/%08d-$i.paths" -p 1 &

  (cd $TMPDIR; \
  inotifywait -q -m -e CLOSE_WRITE --format '%w%f' . \
    | grep --line-buffered -- "/[0-9]*-$i.paths$" \
    | parallel --progress \
      --sshloginfile .. --controlmaster \
      -v --tag --linebuffer \
      --transfer --return "{.}-result.paths" --cleanup \
      "echo Transfer: {}; echo Return: {.}-result.paths; echo pwd: \$PWD; \
      echo ls:; ls; \
      date '+Started: %s.%N (%c)'; \
      touch {.}-result.paths; \
      test -s {} && \$HOME/spa/Release+Asserts/bin/spa-explore \
        --in-paths {} --dont-load-empty-path --out-paths {.}-result.paths \
        ${OPTS[i]}; \
      date '+Finished: %s.%N (%c)';") | tee -a participant-$i.log &
done

inotifywait -q -m -e MOVED_TO --format '%w%f' $TMPDIR \
  | grep --line-buffered -- '-result.paths$' \
  | parallel -j 1 -u cat {} >> $PATH_FILE &

# Start root paths
for i in ${!OPTS[@]}; do
  touch $TMPDIR/root-$i.paths
  (cd $TMPDIR; echo root-$i.paths \
    | parallel --progress \
      --sshloginfile .. --controlmaster \
      -v --tag --linebuffer \
      --transfer --return "{.}-result.paths" --cleanup \
      "echo echo Return: {.}-result.paths; echo pwd: \$PWD; \
      echo ls:; ls; \
      date '+Started: %s.%N (%c)'; \
      \$HOME/spa/Release+Asserts/bin/spa-explore \
        --in-paths {} --out-paths {.}-result.paths ${OPTS[i]}; \
      date '+Finished: %s.%N (%c)';") | tee -a participant-$i.log &
done

# Generate bogus workload to fill up parallel input buffer.
sleep 2
for j in `seq -f %03.0f 1 112`; do
  for i in ${!OPTS[@]}; do
    touch $TMPDIR/$j-$i.paths
  done
done

trap 'kill $(jobs -p); echo "Cleaning up."; rm -rf $TMPDIR; echo "Done."' EXIT

wait
