#!/bin/sh

set -e

make -okj`grep -c processor /proc/cpuinfo` -f Makefile.spa
