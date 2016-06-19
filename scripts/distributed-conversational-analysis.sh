#!/bin/bash

set -e

PATH_FILE="$1"
IFS=';' read -ra OPTS <<< "$2"
IFS=';' read -ra TARGET_CONVERSATIONS <<< "$3"

NUM_PARTICIPANTS=$(echo ${!OPTS[@]} | wc -w)
NUM_TARGET_CONVERSATIONS=$(echo ${!TARGET_CONVERSATIONS[@]} | wc -w)

rm -f $PATH_FILE $PATH_FILE.log
touch $PATH_FILE

echo "Starting conversational analysis with $NUM_PARTICIPANTS participants." \
     | tee -a $PATH_FILE.log
echo "Conversation recorded to $PATH_FILE.paths" | tee -a $PATH_FILE.log

for i in $(seq 0 $((NUM_PARTICIPANTS - 1))); do
  echo "Participant $i analyzed with equivalent of:" | tee -a $PATH_FILE.log
  echo "  spa-explore \\" | tee -a $PATH_FILE.log
  echo "      --in-paths $PATH_FILE \\" | tee -a $PATH_FILE.log
  echo "      --follow-in-paths \\" | tee -a $PATH_FILE.log
  echo "      --out-paths $PATH_FILE \\" | tee -a $PATH_FILE.log
  echo "      --out-paths-append \\" | tee -a $PATH_FILE.log
  echo "      ${OPTS[i]}" | tee -a $PATH_FILE.log
done


declare -A MACHINE_CORES
function getCoresForMachine() {
  if [ ! ${MACHINE_CORES["$1"]} ]; then
    if [ $(cat ~/.parallel/sshloginfile \
               | egrep "^[[:space:]]*[0-9]+/$1") ]; then
      MACHINE_CORES["$1"]=$(cat ~/.parallel/sshloginfile \
                                | egrep "^[[:space:]]*[0-9]+/$1" \
                                | sed -re 's#^[[:space:]]*([0-9]*)/.*#\1#')
    else
      MACHINE_CORES["$1"]=$(ssh "$1" 'grep -c processor /proc/cpuinfo')
    fi
  fi

  echo ${MACHINE_CORES["$1"]}
}

function mkReceiverFile() {
  MACHINE="$1"
  RECEIVE_FILE="$2"

  touch $LOCAL_WORK_DIR/$RECEIVE_FILE
  ssh $MACHINE \
      "rm -f $REMOTE_WORK_DIR/$RECEIVE_FILE; \
      touch $REMOTE_WORK_DIR/$RECEIVE_FILE"
  ssh -n $MACHINE \
      "/bin/bash -O huponexit -c \
               'tail -f $REMOTE_WORK_DIR/$RECEIVE_FILE 2>/dev/null'" \
      > $LOCAL_WORK_DIR/$RECEIVE_FILE &
}

function listJobs() {
  if ls $LOCAL_WORK_DIR/pending/*.paths >/dev/null 2>/dev/null; then
    parallel "awk '
      BEGIN {
        FS = \"_|\t\";
        match_pos = 1;
        depth = 0;
        enable = 0;
        uuid = \"\";
      }
      /^--- SYMBOL LOG START ---$/ {
        enable = 1;
        next;
      }
      /^--- SYMBOL LOG END ---$/ {
        file = FILENAME;
        sub(/^.*\//, \"\", file);
        print depth - match_pos + 1,
              length(target) - match_pos + 1,
              file;
        exit 0;
      }
      {
        if (FNR == NR) {
          target[FNR] = \$0;
        } else if (enable == 1) {
          if (\$1 != uuid) {
            depth++;
            if (target[match_pos] == \$(NF - 1)) {
              match_pos++;
            }
            uuid = \$1;
          }
        }
      }
      END {
      }' \
    {1} {2}" \
      ::: $LOCAL_WORK_DIR/*.conversation ::: $LOCAL_WORK_DIR/pending/*.paths \
      | sort -n \
      | awk '!seen[$3]++ {print $3}'
  fi
}

function runWorker() {
  WORKER_ID="$1"
  MACHINE="$2"
  RESULT_FILE="$3"

  while true; do
    # Atomically get new job.
    JOB_FILE=""
    until [ "$JOB_FILE" ]; do
      if [ ! -d "$LOCAL_WORK_DIR" ]; then
        exit 0
      fi

      JOB_FILE=$(
        (flock 200
          JOB_FILE=$(head -n 1 $LOCAL_WORK_DIR/jobs)

          if [ "$JOB_FILE" ]; then
            # Remove file from consideration so other workers don't get it.
            mv $LOCAL_WORK_DIR/pending/$JOB_FILE \
               $LOCAL_WORK_DIR/running/$JOB_FILE
            tail -n +2 $LOCAL_WORK_DIR/jobs > $LOCAL_WORK_DIR/jobs.new
            mv $LOCAL_WORK_DIR/jobs.new $LOCAL_WORK_DIR/jobs
          fi
          echo $JOB_FILE
        ) 200>$LOCAL_WORK_DIR/lock)

      if [ ! "$JOB_FILE" ]; then
        sleep 1
      fi
    done

    # Run job.
    scp -q $LOCAL_WORK_DIR/running/$JOB_FILE $MACHINE:$REMOTE_WORK_DIR
    PARTICIPANT=$(echo "$JOB_FILE" | sed -re 's/.*-(.*)\.paths/\1/')
    ssh -n $MACHINE \
        "/bin/bash -O huponexit -c \
                  'spa/Release+Asserts/bin/spa-explore \
                    --in-paths $REMOTE_WORK_DIR/$JOB_FILE \
                    --dont-load-empty-path \
                    --out-paths $REMOTE_WORK_DIR/$RESULT_FILE \
                    --out-paths-append \
                    ${OPTS[PARTICIPANT]}'" \
        2>&1 | awk "{print \"[$JOB_FILE] \" \$0}" >> $PATH_FILE.log
    ssh -n $MACHINE \
        rm $REMOTE_WORK_DIR/$JOB_FILE
    rm $LOCAL_WORK_DIR/running/$JOB_FILE
  done
}

function launchWorkersOnMachine() {
  MACHINE="$1"
  NUM_WORKERS="$2"
  FIRST_WORKER_ID="$3"

  for i in \
      $(seq $FIRST_WORKER_ID $((FIRST_WORKER_ID + NUM_WORKERS - 1))); do
    RESULT_FILE="$i-result.paths"
    mkReceiverFile $MACHINE $RESULT_FILE
    RESULT_FILES="$RESULT_FILES $LOCAL_WORK_DIR/$RESULT_FILE"
    runWorker $i $MACHINE $RESULT_FILE &
  done
}

MACHINES=$(cat ~/.parallel/sshloginfile \
               | egrep '^[[:space:]]*[^#]+' \
               | sed -re 's/^\s*([0-9]+\/)?([^#]+).*$/\2/' \
               | tr '\n' ' ')
echo "Running workload on $MACHINES." | tee -a $PATH_FILE.log

echo "Setting up work environment." | tee -a $PATH_FILE.log
LOCAL_WORK_DIR="$(mktemp -d)"
echo "Working in $LOCAL_WORK_DIR" | tee -a $PATH_FILE.log
mkdir $LOCAL_WORK_DIR/split
mkdir $LOCAL_WORK_DIR/pending
mkdir $LOCAL_WORK_DIR/running
touch $LOCAL_WORK_DIR/jobs

REMOTE_WORK_DIR=$(basename $LOCAL_WORK_DIR)
for MACHINE in $MACHINES; do
  ssh $MACHINE "mkdir $REMOTE_WORK_DIR"
done

trap "echo 'Cleaning up.';
      rm -rf $LOCAL_WORK_DIR;
      parallel ssh {} 'rm -rf $REMOTE_WORK_DIR' ::: $MACHINES;
      echo 'Done.';
      trap - SIGTERM && kill -- -$$ 2>/dev/null" SIGINT SIGTERM EXIT

if [ "$NUM_TARGET_CONVERSATIONS" -gt "0" ]; then
  for i in $(seq 0 $((NUM_TARGET_CONVERSATIONS - 1))); do
    echo "Directing exploration towards conversation:" \
         "${TARGET_CONVERSATIONS[i]}" | tee -a $PATH_FILE.log
    echo "${TARGET_CONVERSATIONS[i]}" \
        | tr ' ' '\n' \
        | grep -v '^$' \
        > $LOCAL_WORK_DIR/$i.conversation
  done
else
  # If no target conversation do BFS.
  touch $LOCAL_WORK_DIR/empty.conversation
fi

echo "Starting workers." | tee -a $PATH_FILE.log
WORKER_ID=1
RESULT_FILES=""
for MACHINE in $MACHINES; do
  NUM_CORES=$(getCoresForMachine $MACHINE)
  echo "Running $NUM_CORES workers on $MACHINE." | tee -a $PATH_FILE.log

  launchWorkersOnMachine $MACHINE $NUM_CORES $WORKER_ID
  WORKER_ID=$((WORKER_ID + NUM_CORES))
done
echo "Running a total of $((WORKER_ID - 1)) workers across all machines." \
     | tee -a $PATH_FILE.log

echo "Starting path splitting." | tee -a $PATH_FILE.log
spaSplitPaths \
    -i $PATH_FILE -f \
    -o "$LOCAL_WORK_DIR/split/%06d.paths" \
    -p 1 \
    >> $PATH_FILE.log 2>&1 &

(while true; do
  for f in $(ls -1 $LOCAL_WORK_DIR/split/*.paths 2>/dev/null); do
    for p in $(seq 0 $((NUM_PARTICIPANTS - 1))); do
      cp $f $LOCAL_WORK_DIR/pending/$(basename $f .paths)-$p.paths
    done
    rm $f

    # Recompute prioritized job list.
    (flock 200
      listJobs > $LOCAL_WORK_DIR/jobs
    ) 200>$LOCAL_WORK_DIR/lock
  done
  sleep 1
done) &

INITIAL_MACHINE=$(echo "$MACHINES" | awk '{print $1}')
echo "Starting initial jobs on $INITIAL_MACHINE." | tee -a $PATH_FILE.log
for PARTICIPANT in $(seq 0 $((NUM_PARTICIPANTS - 1))); do
  mkReceiverFile $INITIAL_MACHINE root-$PARTICIPANT-result.paths
  RESULT_FILES="$RESULT_FILES $LOCAL_WORK_DIR/root-$PARTICIPANT-result.paths"
  touch $LOCAL_WORK_DIR/running/root-$PARTICIPANT.paths
  (ssh -n $INITIAL_MACHINE \
      "/bin/bash -O huponexit -c \
               'spa/Release+Asserts/bin/spa-explore \
                  --in-paths /dev/null \
                  --out-paths $REMOTE_WORK_DIR/root-$PARTICIPANT-result.paths \
                  ${OPTS[PARTICIPANT]}; \
                echo --- Done ---'" \
        2>&1 | awk "{print \"[root-$PARTICIPANT] \" \$0}" >> $PATH_FILE.log;
  rm $LOCAL_WORK_DIR/running/root-$PARTICIPANT.paths) &
done

echo "Starting path collection." | tee -a $PATH_FILE.log
spaJoinPaths \
    -f $RESULT_FILES \
    $PATH_FILE \
    >> $PATH_FILE.log 2>&1 &

echo "Waiting for exploration to complete." | tee -a $PATH_FILE.log
START_TIME=$(date +%s)
NO_ACTIVITY_COUNT=0
while true; do
  NUM_FOUND=$(grep -c -- '--- PATH START ---' $PATH_FILE || true)
  NUM_RUNNING=$(ls -1 $LOCAL_WORK_DIR/running/*.paths 2>/dev/null | wc -l)
  NUM_PENDING=$(ls -1 $LOCAL_WORK_DIR/pending/*.paths 2>/dev/null | wc -l)

  echo "[$(($(date +%s) - START_TIME))]" \
       "Found $NUM_FOUND paths." \
       "Processing $NUM_RUNNING paths." \
       "$NUM_PENDING paths pending." | tee -a $PATH_FILE.log

  if [ "$((NUM_RUNNING + NUM_PENDING))" -eq "0" ]; then
    NO_ACTIVITY_COUNT=$((NO_ACTIVITY_COUNT + 1))
  else
    NO_ACTIVITY_COUNT=0
  fi

  if [ "$NO_ACTIVITY_COUNT" -ge "5" ]; then
    break
  fi

  sleep 1
done
echo "Exploration completed." | tee -a $PATH_FILE.log
