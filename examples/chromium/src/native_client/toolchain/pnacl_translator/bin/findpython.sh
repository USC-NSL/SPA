#!/bin/sh
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Keep this script compatible with different shells.
# It has been tested to work with: bash, zsh, ksh93, and dash.

PYTHON="${PYTHON:-notset}"

if test "${PYTHON}" = "notset" ; then
  for pypath in python2.6 \
                python2.7 \
                python2.5 \
                python \
                /usr/bin/python2.6 \
                /usr/bin/python2.7 \
                /usr/bin/python2.5 \
                /usr/bin/python ; do
    if "${pypath}" --version 2>&1 | grep -q "Python 2.[5-7]"; then
      PYTHON="${pypath}"
      break
    fi
  done
  if test "${PYTHON}" = "notset" ; then
    echo "findpython.sh:" \
         "Can't find python 2.5, 2.6 or 2.7." \
         "Please set environment variable PYTHON." 1>&2
    exit 1
  fi
fi
