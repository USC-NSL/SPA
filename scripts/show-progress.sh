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

if [ "$PATH_FILE" -nt "derived.csv" ]; then
  $CAT $PATH_FILE | awk '
    BEGIN {
      FS = "\t";
      enable = 0;
    }
    /^--- SYMBOL LOG START ---$/ {
      enable = 1;
      derived = 0;
      next;
    }
    /^--- SYMBOL LOG END ---$/ {
      print derived;
      enable = 0;
      next;
    }
    {
      if (enable == 1) {
        derived = ($2 != "");
      }
    }' > derived.csv
fi

DISPLAY="" octave -q <<EOF
  progress = csvread('progress.csv');
  derived = csvread('derived.csv');

  time = [0];
  num_derived = [0];
  num_paths = [0];
  for i = 1:length(derived)
    if (sum(progress(:,2) >= i) > 0)
      time(end+1) = progress(progress(:,2) >= i, 1)(1);
      num_derived(end+1) = num_derived(end) + derived(i);
      num_paths(end+1) = num_paths(end) + 1;
    endif
  endfor

  %cutoff = 2282;
  %time = time(1:cutoff);
  %num_derived = num_derived(1:cutoff);
  %num_paths = num_paths(1:cutoff);

  %plot(time / 60 / 60 / 24, num_paths, "-;Total;", time / 60 / 60 / 24, num_derived, "--;Derived;");
  hold on;
  area(time / 60 / 60 / 24, num_paths, "FaceColor", "cyan");
  area(time / 60 / 60 / 24, num_derived, "FaceColor", "blue");

  xlabel("Time (Days)");
  ylabel("C-Paths");
  legend("Total", "Derived", "location", "northwest");
  axis([0, max(time / 60 / 60 / 24)]);

  print("-S300,150", "paths-time.pdf");
  quit;
EOF

# rm -f progress.csv derived.csv
