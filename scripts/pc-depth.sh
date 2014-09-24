#!/bin/bash

spaSplitPaths -i $1 -o %06d.paths -p 1

parallel -j 200% -k "grep '^(query \[(\|^         (' {} | grep -v '(Eq (Read w8 [0-9]* spa_in_msg_request)' | wc -l" ::: ??????.paths > pc-depth.csv
rm ??????.paths

octave -qf pc-depth-cdf.m
rm pc-depth.csv
