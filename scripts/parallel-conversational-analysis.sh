#!/bin/bash

set -ex

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
spaSplitPaths -i $PATH_FILE -f -o "$TMPDIR/%08d.paths" -p 1 &

for i in ${!OPTS[@]}; do
  rm -f participant-$i.log
  (cd $TMPDIR; \
  inotifywait -q -m -e CLOSE_WRITE --format '%w%f' . \
    | grep --line-buffered -- '/[0-9]*.paths$' \
    | parallel --progress \
      --sshloginfile .. --controlmaster \
      -v --tag --linebuffer \
      --transfer --return "{.}-$i-result.paths" --cleanup \
      "echo Transfer: {}; echo Return: {.}-$i-result.paths; echo pwd: \$PWD; \
      echo ls:; ls; \
      date '+Started: %s.%N (%c)'; \
      test -s {} && \$HOME/spa/Release+Asserts/bin/spa-explore \
        --in-paths {} --dont-load-empty-path --out-paths {.}-$i-result.paths \
        ${OPTS[i]}; \
      date '+Finished: %s.%N (%c)';") | tee -a participant-$i.log &
done

inotifywait -q -m -e MOVED_TO --format '%w%f' $TMPDIR \
  | grep --line-buffered -- '-result.paths$' \
  | parallel -j 1 -u cat {} >> $PATH_FILE &

# Start root paths
for i in ${!OPTS[@]}; do
  (cd $TMPDIR; echo root-$i \
    | parallel --progress \
      --sshloginfile .. --controlmaster \
      -v --tag --linebuffer \
      --transfer --return "{.}-result.paths" --cleanup \
      "echo echo Return: {.}-result.paths; echo pwd: \$PWD; \
      echo ls:; ls; \
      date '+Started: %s.%N (%c)'; \
      \$HOME/spa/Release+Asserts/bin/spa-explore \
        --out-paths {.}-result.paths ${OPTS[i]}; \
      date '+Finished: %s.%N (%c)';") | tee -a participant-$i.log &
done

trap 'kill $(jobs -p)' SIGINT
wait

echo "Cleaning up."
# rm -rf $TMPDIR

echo "Done."
