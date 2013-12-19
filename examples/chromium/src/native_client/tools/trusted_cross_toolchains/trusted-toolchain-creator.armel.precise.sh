#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script builds the (trusted) cross toolchain for arm.
#@ It must be run from the native_client/ directory.
#@
#@ The toolchain consists primarily of a jail with arm header and libraries.
#@ It also provides additional tools such as QEMU.
#@ It does NOT provide the actual cross compiler anymore.
#@ The cross compiler is now comming straight from a debian package.
#@ So there is a one-time step required for all machines using this TC.
#@ Which is especially true for build-bots:
#@
#@  tools/trusted_cross_toolchains/trusted-toolchain-creator.armel.precise.sh  InstallCrossArmBasePackages
#@
#@
#@  Generally this script is invoked as:
#@  tools/trusted_cross_toolchains/trusted-toolchain-creator.armel.precise.sh <mode> <args>*
#@  Available modes are shown below.
#@
#@
#@ This Toolchain was tested with Ubuntu Lucid
#@
#@ Usage of this TC:
#@  compile: arm-linux-gnueabi-gcc -march=armv7-a -isystem ${JAIL}/usr/include
#@  link:    arm-linux-gnueabi-gcc -L${JAIL}/usr/lib -L${JAIL}/usr/lib/arm-linux-gnueabi
#@                                 -L${JAIL}/lib -L${JAIL}/lib/arm-linux-gnueabi
#@
#@ Usage of QEMU
#@  TBD
#@
#@ List of modes:

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly SCRIPT_DIR=$(dirname $0)

# this where we create the ARMEL "jail"
readonly INSTALL_ROOT=$(pwd)/toolchain/linux_arm-trusted

readonly TMP=/tmp/arm-crosstool-precise

readonly REQUIRED_TOOLS="wget"

readonly MAKE_OPTS="-j8"

######################################################################
# Package Config
######################################################################

# this where we get the cross toolchain from for the manual install:
readonly CROSS_ARM_TC_REPO=http://archive.ubuntu.com/ubuntu
# this is where we get all the armel packages from
readonly ARMEL_REPO=http://ports.ubuntu.com/ubuntu-ports

readonly PACKAGE_LIST="${ARMEL_REPO}/dists/precise/main/binary-armel/Packages.bz2"

# Packages for the host system
# NOTE: at one point we should get rid of the 4.5 packages
readonly CROSS_ARM_TC_PACKAGES="\
  libc6-armel-cross \
  libc6-dev-armel-cross \
  libgcc1-armel-cross \
  libgomp1-armel-cross \
  linux-libc-dev-armel-cross \
  libgcc1-dbg-armel-cross \
  libgomp1-dbg-armel-cross \
  binutils-arm-linux-gnueabi \
  cpp-arm-linux-gnueabi \
  gcc-arm-linux-gnueabi \
  g++-arm-linux-gnueabi \
  cpp-4.5-arm-linux-gnueabi \
  gcc-4.5-arm-linux-gnueabi \
  g++-4.5-arm-linux-gnueabi \
  libmudflap0-dbg-armel-cross
"

# Jail packages: these are good enough for native client
# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMEL_BASE_PACKAGES="\
  libssl-dev \
  libssl1.0.0 \
  libgcc1 \
  libc6 \
  libc6-dev \
  libstdc++6 \
  libx11-dev \
  libx11-6 \
  x11proto-core-dev \
  libxt-dev \
  libxt6 \
  zlib1g \
  zlib1g-dev"

# Additional jail packages needed to build chrome
# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMEL_BASE_DEP_LIST="${SCRIPT_DIR}/packagelist.precise.armel.base"
readonly ARMEL_BASE_DEP_FILES="$(cat ${ARMEL_BASE_DEP_LIST})"

readonly ARMEL_EXTRA_PACKAGES="\
  comerr-dev \
  krb5-multidev \
  libasound2 \
  libasound2-dev \
  libatk1.0-0 \
  libatk1.0-dev \
  libbz2-1.0 \
  libbz2-dev \
  libcairo2 \
  libcairo2-dev \
  libcairo-gobject2 \
  libcairo-script-interpreter2 \
  libcomerr2 \
  libcups2 \
  libcups2-dev \
  libdbus-1-3 \
  libdbus-1-dev \
  libexpat1 \
  libexpat1-dev \
  libfontconfig1 \
  libfontconfig1-dev \
  libfreetype6 \
  libfreetype6-dev \
  libgconf-2-4 \
  libgconf2-4 \
  libgconf2-dev \
  libgpg-error0 \
  libgpg-error-dev \
  libgcrypt11 \
  libgcrypt11-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libgnutls26 \
  libgnutlsxx27 \
  libgnutls-dev \
  libgnutls-openssl27 \
  libgssapi-krb5-2 \
  libgssrpc4 \
  libgtk2.0-0 \
  libgtk2.0-dev \
  libglib2.0-0 \
  libglib2.0-dev \
  libgnome-keyring0 \
  libgnome-keyring-dev \
  libkadm5clnt-mit8 \
  libkadm5srv-mit8 \
  libkdb5-6 \
  libkrb5-3 \
  libkrb5-dev \
  libkrb5support0 \
  libk5crypto3 \
  libnspr4 \
  libnspr4-dev \
  libnss3 \
  libnss3-dev \
  libnss-db \
  liborbit2 \
  libpam0g \
  libpam0g-dev \
  libpango1.0-0 \
  libpango1.0-dev \
  libpci3 \
  libpci-dev \
  libpcre3 \
  libpcre3-dev \
  libpcrecpp0 \
  libpixman-1-0 \
  libpixman-1-dev \
  libpng12-0 \
  libpng12-dev \
  libpulse0 \
  libpulse-dev \
  libpulse-mainloop-glib0 \
  libselinux1 \
  libspeechd2 \
  libspeechd-dev \
  libudev0 \
  libudev-dev \
  libxext-dev \
  libxext6 \
  libxau-dev \
  libxau6 \
  libxcb1 \
  libxcb1-dev \
  libxcb-render0 \
  libxcb-render0-dev \
  libxcb-shm0 \
  libxcb-shm0-dev \
  libxcomposite1 \
  libxcomposite-dev \
  libxcursor1 \
  libxcursor-dev \
  libxdamage1 \
  libxdamage-dev \
  libxdmcp6 \
  libxfixes3 \
  libxfixes-dev \
  libxi6 \
  libxi-dev \
  libxinerama1 \
  libxinerama-dev \
  libxrandr2 \
  libxrandr-dev \
  libxrender1 \
  libxrender-dev \
  libxss1 \
  libxss-dev \
  libxtst6 \
  libxtst-dev \
  speech-dispatcher \
  x11proto-composite-dev \
  x11proto-damage-dev \
  x11proto-fixes-dev \
  x11proto-input-dev \
  x11proto-kb-dev \
  x11proto-randr-dev \
  x11proto-record-dev \
  x11proto-render-dev \
  x11proto-scrnsaver-dev \
  x11proto-xext-dev"

# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMEL_EXTRA_DEP_LIST="${SCRIPT_DIR}/packagelist.precise.armel.extra"
readonly ARMEL_EXTRA_DEP_FILES="$(cat ${ARMEL_EXTRA_DEP_LIST})"

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


SubBanner() {
  echo "......................................................................"
  echo $*
  echo "......................................................................"
}


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}


DownloadOrCopy() {
  if [[ -f "$2" ]] ; then
     echo "$2 already in place"
  elif [[ $1 =~  'http://' ]] ; then
    SubBanner "downloading from $1 -> $2"
    wget $1 -O $2
  else
    SubBanner "copying from $1"
    cp $1 $2
  fi
}


# some sanity checks to make sure this script is run from the right place
# with the right tools
SanityCheck() {
  Banner "Sanity Checks"

  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi

  if ! mkdir -p "${INSTALL_ROOT}" ; then
     echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit -1
  fi

  if ! mkdir -p "${TMP}" ; then
     echo "ERROR: ${TMP} can't be created."
    exit -1
  fi

  for tool in ${REQUIRED_TOOLS} ; do
    if ! which ${tool} ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done
}


ChangeDirectory() {
  # Change direcotry to top 'native_client' directory.
  cd $(dirname ${BASH_SOURCE})
  cd ../..
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball} -C ${INSTALL_ROOT} .
}

######################################################################
# One of these has to be run ONCE per machine
######################################################################

#@
#@ InstallCrossArmBasePackages
#@
#@      Install packages needed for arm cross compilation.
InstallCrossArmBasePackages() {
  sudo apt-get install ${CROSS_ARM_TC_PACKAGES}
}

######################################################################
#
######################################################################

#@
#@ InstallTrustedLinkerScript
#@
#@     This forces the loading address of sel_ldr like programs
#@     to higher memory areas where they do not conflict with
#@     untrusted binaries.
#@     This likely no longer used because of "nacl_helper_bootstrap".
InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/ld_script_arm_trusted
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  arm-linux-gnueabi-ld  --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/00008000/70000000/g' > ${trusted_ld_script}
}

HacksAndPatches() {
  rel_path=toolchain/linux_arm-trusted
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${rel_path}/usr/lib/arm-linux-gnueabi/libpthread.so \
            ${rel_path}/usr/lib/arm-linux-gnueabi/libc.so"

  SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/arm-linux-gnueabi/||g'  ${lscripts}
  sed -i -e 's|/lib/arm-linux-gnueabi/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${rel_path}/usr/share
  ln -s ../lib/arm-linux-gnueabi/pkgconfig ${rel_path}/usr/share/pkgconfig
}


InstallMissingArmLibrariesAndHeadersIntoJail() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${TMP}/armel-packages
  mkdir -p ${INSTALL_ROOT}
  for file in $@ ; do
    local package="${TMP}/armel-packages/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${ARMEL_REPO}/pool/${file} ${package}
    SubBanner "extracting to ${INSTALL_ROOT}"
    if [[ ! -s ${package} ]] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit -1
    fi
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${INSTALL_ROOT}
  done
}


CleanupJailSymlinks() {
  Banner "jail symlink cleanup"

  pushd ${INSTALL_ROOT}
  find usr/lib -type l -printf '%p %l\n' | while read link target; do
    # skip links with non-absolute paths
    if [[ ${target} != /* ]] ; then
      continue
    fi
    echo "${link}: ${target}"
    case "${link}" in
      usr/lib/arm-linux-gnueabi/*)
        # Relativize the symlink.
        ln -snfv "../../..${target}" "${link}"
        ;;
      usr/lib/*)
        # Relativize the symlink.
        ln -snfv "../..${target}" "${link}"
        ;;
    esac
  done

  find usr/lib -type l -printf '%p %l\n' | while read link target; do
    # Make sure we catch new bad links.
    # libnss_db.so is an exception this since is actually a broken link
    # in Ubuntu. See /usr/lib/x86_64-linux-gnu/libnss_db.so on a
    # precise desktop.
    # TODO(sbc): remove this exception if/when Ubuntu fixes this link.
    if [ "${link}" == "usr/lib/arm-linux-gnueabi/libnss_db.so" ] ; then
      echo "ignoring known bad link: ${link}"
    elif [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      exit -1
    fi
  done
  popd
}

#@
#@ BuildAndInstallQemu
#@
#@     Build ARM emulator including some patches for better tracing
#
# Historic Notes:
# Traditionally we were builidng static 32 bit images of qemu on a
# 64bit system which would run then on both x86-32 and x86-64 systems.
# The latest version of qemu contains new dependencies which
# currently make it impossible to build such images on 64bit systems
# We can build a static 64bit qemu but it does not work with
# the sandboxed translators for unknown reason.
# So instead we chose to build 32bit shared images.
#

readonly QEMU_TARBALL=$(readlink -f ../third_party/qemu/qemu-1.0.1.tar.gz)
readonly QEMU_PATCH=$(readlink -f ../third_party/qemu/qemu-1.0.1.patch_arm)
readonly QEMU_DIR=qemu-1.0.1

BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu.nacl"

  Banner "Building qemu in ${tmpdir}"

  if [[ -z "$QEMU_TARBALL" ]] ; then
    echo "ERROR: missing qemu tarball: ../third_party/qemu/qemu-1.0.1.tar.gz"
    exit 1
  fi

  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}
  SubBanner "Untaring ${QEMU_TARBALL}"
  tar zxf ${QEMU_TARBALL}
  cd ${QEMU_DIR}

  SubBanner "Patching ${QEMU_PATCH}"
  patch -p1 < ${QEMU_PATCH}

  SubBanner "Configuring"
  env -i PATH=/usr/bin/:/bin \
    ./configure \
    --extra-cflags="-m32" \
    --extra-ldflags="-Wl,-rpath=/lib32" \
    --disable-system \
    --enable-linux-user \
    --disable-darwin-user \
    --disable-bsd-user \
    --target-list=arm-linux-user \
    --disable-smartcard-nss \
    --disable-sdl

# see above for why we can no longer use -static
#    --static

  SubBanner "Make"
  env -i PATH=/usr/bin/:/bin \
      V=99 make MAKE_OPTS=${MAKE_OPTS}

  SubBanner "Install ${INSTALL_ROOT}"
  cp arm-linux-user/qemu-arm ${INSTALL_ROOT}
  cd ${saved_dir}
  cp tools/trusted_cross_toolchains/qemu_tool_arm.sh ${INSTALL_ROOT}
  ln -sf qemu_tool_arm.sh ${INSTALL_ROOT}/run_under_qemu_arm
}

#@
#@ BuildJail <tarball-name>
#@
#@    Build everything and package it
BuildJail() {
  ClearInstallDir
  InstallMissingArmLibrariesAndHeadersIntoJail \
    ${ARMEL_BASE_DEP_FILES} \
    ${ARMEL_EXTRA_DEP_FILES}
  CleanupJailSymlinks
  InstallTrustedLinkerScript
  HacksAndPatches
  BuildAndInstallQemu
  CreateTarBall $1
}

#
# GeneratePackageList
#
#     Looks up package names in ${TMP}/Packages and write list of URLs
#     to output file.
#
GeneratePackageList() {
  local output_file=$1
  echo "Updating: ${output_file}"
  /bin/rm -f ${output_file}
  shift
  for pkg in $@ ; do
    local pkg_full=$(grep -A 1 "${pkg}\$" ${TMP}/Packages | egrep -o "pool/.*")
    if [[ -z ${pkg_full} ]]; then
        echo "ERROR: missing package: $pkg"
        exit 1
    fi
    echo $pkg_full | sed "s/^pool\///" >> $output_file
  done
  # sort -o does an in-place sort of this file
  sort $output_file -o $output_file
}

#@
#@ UpdatePackageLists
#@
#@     Regenerate the armel package lists such that they contain an up-to-date
#@     list of URLs within the ubuntu archive.
#@
UpdatePackageLists() {
  local package_list="${TMP}/Packages.precise.bz2"
  DownloadOrCopy ${PACKAGE_LIST} ${package_list}
  bzcat ${package_list} | egrep '^(Package:|Filename:)' > ${TMP}/Packages

  GeneratePackageList ${ARMEL_BASE_DEP_LIST} "${ARMEL_BASE_PACKAGES}"
  GeneratePackageList ${ARMEL_EXTRA_DEP_LIST} "${ARMEL_EXTRA_PACKAGES}"
}

if [[ $# -eq 0 ]] ; then
  echo "ERROR: you must specify a mode on the commandline"
  echo
  Usage
  exit -1
elif [[ "$(type -t $1)" != "function" ]]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  ChangeDirectory
  SanityCheck
  "$@"
fi
