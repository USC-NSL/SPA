#!/bin/bash

if [[ ${1##*.} == "bz2" ]]; then
  CAT='bzcat'
  PATH_FILE="$1"
  LOG_FILE="${1%.*}.log.bz2"
else
  CAT='cat'
  PATH_FILE="$1"
  LOG_FILE="$1.log"
fi

if [ "$LOG_FILE" -nt "progress.csv" ]; then
  $CAT $LOG_FILE \
    | egrep '^\[[0-9]*\] Found [0-9]* paths. Processing [0-9]* paths. [0-9]* paths pending.' \
    | sed 's/^\[\([0-9]*\)\] Found \([0-9]*\) paths. Processing [0-9]* paths. [0-9]* paths pending..*$/\1,\2/' \
    > progress.csv
fi

if [ "$PATH_FILE" -nt "depths.csv" ]; then
  $CAT $PATH_FILE | awk '
    BEGIN {
      FS = "_|\t";
      depth = 0;
      enable = 0;
      uuid = "";
    }
    /^--- SYMBOL LOG START ---$/ {
      enable = 1;
      next;
    }
    /^--- SYMBOL LOG END ---$/ {
      print depth;
      depth = 0;
      enable = 0;
      uuid = "";
      next;
    }
    {
      if (enable == 1) {
        if ($1 != uuid) {
          depth++;
          uuid = $1;
        }
      }
    }' > depths.csv
fi

DISPLAY="" octave -q <<EOF
  progress = csvread('progress.csv');
  depths = csvread('depths.csv');

  time = [];
  depth = [];
  min_depth = [];
  for i = 1:length(depths)
    if (sum(progress(:,2) >= i) > 0)
      time(end+1) = progress(progress(:,2) >= i, 1)(1);
      depth(end+1) = depths(i);
      min_depth(end+1) = min(depths(i:end));
    endif
  endfor

  %time = time(1:7773);
  %depth = depth(1:7773);
  %min_depth = min_depth(1:7773);

  plot(time / 60 / 60 / 24, depth, ".", time / 60 / 60 / 24, min_depth, "r-", "LineWidth", 3);
  xlabel("Time (Days)");
  ylabel("Message Depth");
  axis([0, max(time / 60 / 60 / 24)]);

  print("-S300,150", "depth-time.pdf");
  quit;
EOF

# rm -f progress.csv depth.csv
