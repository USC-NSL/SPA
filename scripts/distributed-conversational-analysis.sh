#!/bin/bash

set -e

PATH_FILE="$1"
IFS=';' read -ra OPTS <<< "$2"

rm -f $PATH_FILE
touch $PATH_FILE

NUM_PARTICIPANTS=$(echo ${!OPTS[@]} | wc -w)
echo "Starting conversational analysis with $NUM_PARTICIPANTS participants."
echo "Conversation recorded to $PATH_FILE.paths"

for i in $(seq 1 $NUM_PARTICIPANTS); do
  echo "Participant $i analyzed with equivalent of:"
  echo "  spa-explore \\"
  echo "      --in-paths $PATH_FILE \\"
  echo "      --follow-in-paths \\"
  echo "      --out-paths $PATH_FILE \\"
  echo "      --out-paths-append \\"
  echo "      ${OPTS[i]}"
done


declare -A MACHINE_CORES
function getCoresForMachine() {
  if [ ! ${MACHINE_CORES["$1"]} ]; then
    MACHINE_CORES["$1"]=$(ssh "$1" 'grep -c processor /proc/cpuinfo')
  fi

  echo ${MACHINE_CORES["$1"]}
}

function mkSenderFile() {
  MACHINE="$1"
  SEND_FILE="$2"

  rm -f $LOCAL_WORK_DIR/$SEND_FILE
  touch $LOCAL_WORK_DIR/$SEND_FILE
  tail -f $LOCAL_WORK_DIR/$SEND_FILE \
      2>/dev/null \
      | ssh -tt $MACHINE \
            "/bin/bash -O huponexit -c \
                     'cat > $REMOTE_WORK_DIR/$SEND_FILE'" \
      >/dev/null &
}

function mkReceiverFile() {
  MACHINE="$1"
  RECEIVE_FILE="$2"

  ssh $MACHINE \
      "rm -f $REMOTE_WORK_DIR/$RECEIVE_FILE; \
      touch $REMOTE_WORK_DIR/$RECEIVE_FILE"
  ssh -tt $MACHINE \
      "/bin/bash -O huponexit -c \
               'tail -f $REMOTE_WORK_DIR/$RECEIVE_FILE 2>/dev/null'" \
      > $LOCAL_WORK_DIR/$RECEIVE_FILE &
}

MACHINES=$(cat ~/.parallel/sshloginfile \
               | egrep '^[[:space:]]*[^#]+' \
               | sed -re 's/^\s*([^#]+).*$/\1/')

echo "Setting up work environment."
LOCAL_WORK_DIR="$(mktemp -d)"
echo "Working in $LOCAL_WORK_DIR"
REMOTE_WORK_DIR=$(basename $LOCAL_WORK_DIR)
for MACHINE in $MACHINES; do \
  ssh $MACHINE "mkdir $REMOTE_WORK_DIR"
done
for PARTICIPANT in $(seq 0 $((NUM_PARTICIPANTS - 1))); do \
  rm -f participant-$PARTICIPANT.log
done

trap "echo 'Cleaning up.'; \
      rm -rf $LOCAL_WORK_DIR; \
      for MACHINE in $MACHINES; do \
        ssh $MACHINE 'rm -rf $REMOTE_WORK_DIR'; \
      done; \
      echo 'Done.'; \
      trap - SIGTERM && kill -- -$$ 2>/dev/null" SIGINT SIGTERM EXIT

echo "Starting long term jobs."
NUM_JOBS=0
RESULT_FILES=""
for MACHINE in $MACHINES; do \
  NUM_CORES=$(getCoresForMachine $MACHINE)
  echo "Running $NUM_CORES jobs on $MACHINE."

  for i in $(seq 1 $NUM_CORES); do \
    NUM_JOBS=$((NUM_JOBS + 1))

    mkSenderFile $MACHINE $NUM_JOBS.paths

    for PARTICIPANT in $(seq 0 $((NUM_PARTICIPANTS - 1))); do \
      mkReceiverFile $MACHINE $NUM_JOBS-$PARTICIPANT-result.paths
      RESULT_FILES="$RESULT_FILES $LOCAL_WORK_DIR/$NUM_JOBS-$PARTICIPANT-result.paths"
      echo "[Job $NUM_JOBS] Running: ssh -tt $MACHINE" \
            "/bin/bash -O huponexit -c" \
                     "'spa/Release+Asserts/bin/spa-explore" \
                     "--in-paths $REMOTE_WORK_DIR/$NUM_JOBS.paths" \
                     "--dont-load-empty-path" \
                     "--follow-in-paths" \
                     "--out-paths $REMOTE_WORK_DIR/$NUM_JOBS-$PARTICIPANT-result.paths" \
                     "${OPTS[PARTICIPANT]}'" \
           >> participant-$PARTICIPANT.log
      ssh -tt $MACHINE \
          "/bin/bash -O huponexit -c \
                   'spa/Release+Asserts/bin/spa-explore \
                      --in-paths $REMOTE_WORK_DIR/$NUM_JOBS.paths \
                      --dont-load-empty-path \
                      --follow-in-paths \
                      --out-paths $REMOTE_WORK_DIR/$NUM_JOBS-$PARTICIPANT-result.paths \
                      ${OPTS[PARTICIPANT]}'" \
          2>&1 | tee $LOCAL_WORK_DIR/$NUM_JOBS-$PARTICIPANT.log \
          | awk "{print \"[Job $NUM_JOBS] \" \$0}" \
          >> participant-$PARTICIPANT.log &
    done
  done
done
echo "Running a total of $((NUM_JOBS * NUM_PARTICIPANTS)) tasks across all machines."

echo "Starting path splitting."
spaSplitPaths \
    -i $PATH_FILE -f \
    -o "$LOCAL_WORK_DIR/%d.paths" \
    -n $NUM_JOBS \
    > $LOCAL_WORK_DIR/spaSplitPaths.log 2>&1 &

INITIAL_MACHINE=$(echo "$MACHINES" | head -n 1)
echo "Starting initial jobs on $INITIAL_MACHINE."
for PARTICIPANT in $(seq 0 $((NUM_PARTICIPANTS - 1))); do \
  mkReceiverFile $INITIAL_MACHINE root-$PARTICIPANT-result.paths
  RESULT_FILES="$RESULT_FILES $LOCAL_WORK_DIR/root-$PARTICIPANT-result.paths"
  echo "[Job root] Running: ssh -tt $INITIAL_MACHINE" \
                    "/bin/bash -O huponexit -c" \
                    "'spa/Release+Asserts/bin/spa-explore" \
                    "--in-paths /dev/null" \
                    "--out-paths $REMOTE_WORK_DIR/root-$PARTICIPANT-result.paths" \
                    "${OPTS[PARTICIPANT]}'" \
       >> participant-$PARTICIPANT.log
  ssh -tt $INITIAL_MACHINE \
      "/bin/bash -O huponexit -c \
               'spa/Release+Asserts/bin/spa-explore \
                  --in-paths /dev/null \
                  --out-paths $REMOTE_WORK_DIR/root-$PARTICIPANT-result.paths \
                  ${OPTS[PARTICIPANT]}'" \
        2>&1 | tee $LOCAL_WORK_DIR/root-$PARTICIPANT.log \
        | awk '{print "[Job root] " $0}' \
        >> participant-$PARTICIPANT.log &
done

echo "Starting path collection."
spaJoinPaths \
    -f $RESULT_FILES \
    $PATH_FILE \
    > $LOCAL_WORK_DIR/spaJoinPaths.log 2>&1 &

echo "Waiting for exploration to complete."
START_TIME=$(date +%s)
NO_ACTIVITY_COUNT=0
runtime=$((end-start))
while true; do \
  NUM_STATES=$(parallel "tac {} | egrep -m 1 \
'(KLEE: \[SpaSearcher\] Queued:)'\
'|(^.*KLEE: Done\..*$)'\
'|(KLEE: \[spa_load_path\]     Path .* not yet available\. Waiting\..*$)'" \
                        ::: $LOCAL_WORK_DIR/*-*.log \
                 | egrep '(KLEE: \[SpaSearcher\] Queued:)' \
                 | awk 'BEGIN {s = 0} \
                        {s += $5} \
                        END {print s}')

  echo "[$(($(date +%s) - START_TIME))]" \
       "Found $(grep -c -- '--- PATH START ---' $PATH_FILE) paths." \
       "Exploring $NUM_STATES states."

  if [ ! "$(tail -n 1 $LOCAL_WORK_DIR/*-*.log \
                | egrep -v \
'(==> .*\.log <==)'\
'|(^$)'\
'|(KLEE: \[spa_load_path\]     Path .* not yet available\. Waiting\.)'\
'|(Connection to .* closed\.)')" ]; then
    NO_ACTIVITY_COUNT=$((NO_ACTIVITY_COUNT + 1))
  else
    NO_ACTIVITY_COUNT=0
  fi

  if [ "$NO_ACTIVITY_COUNT" -ge "5" ]; then
    break
  fi

  sleep 1
done
echo "Exploration completed."
