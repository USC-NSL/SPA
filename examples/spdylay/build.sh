#!/bin/sh

set -e

make -kj`grep -c processor /proc/cpuinfo`
