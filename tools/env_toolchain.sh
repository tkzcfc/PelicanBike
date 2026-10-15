#!/bin/sh
# UNBAKE-1 desktop toolchain bootstrap.
#
# This container ships gcc (C frontend only) with NO g++/cc1plus, NO make and NO X11/GL
# dev headers, so the mandated desktop build cannot run as-is. Since we have no root
# (no sudo, apt-get refuses to write /var/lib/apt), the missing Debian trixie packages
# are staged into a private root via dpkg-deb -x (see fetch_debs.py / extract_debs.sh)
# and wired in through compiler/cmake search-path env vars. Nothing outside $HOME is
# touched; deleting /home/box/.tc reverts everything.
#
# Source this file before any cmake configure/build.
TC=/home/box/.tc

PATH="$TC/usr/bin:$PATH"; export PATH
# gcc/g++ driver search paths for subprograms (cc1plus, collect2, liblto_plugin) and libs
COMPILER_PATH="$TC/usr/lib/gcc/x86_64-linux-gnu/14:$TC/usr/libexec/gcc/x86_64-linux-gnu/14"
export COMPILER_PATH
CPATH="$TC/usr/include:$TC/usr/lib/gcc/x86_64-linux-gnu/14/include"
export CPATH
LIBRARY_PATH="$TC/usr/lib/x86_64-linux-gnu:$TC/usr/lib/gcc/x86_64-linux-gnu/14"
export LIBRARY_PATH
C_INCLUDE_PATH="$CPATH"; export C_INCLUDE_PATH
CPLUS_INCLUDE_PATH="$CPATH"; export CPLUS_INCLUDE_PATH
LD_LIBRARY_PATH="$TC/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"; export LD_LIBRARY_PATH
PKG_CONFIG_PATH="$TC/usr/lib/x86_64-linux-gnu/pkgconfig:$TC/usr/share/pkgconfig"
export PKG_CONFIG_PATH
PKG_CONFIG_SYSROOT_DIR="$TC"; export PKG_CONFIG_SYSROOT_DIR

# cmake/gcc defaults so the build never has to guess
CC="$TC/usr/bin/gcc-14"; export CC
CXX="$TC/usr/bin/g++-14"; export CXX
CMAKE_PREFIX_PATH="$TC/usr"; export CMAKE_PREFIX_PATH
CMAKE_INCLUDE_PATH="$TC/usr/include"; export CMAKE_INCLUDE_PATH
CMAKE_LIBRARY_PATH="$TC/usr/lib/x86_64-linux-gnu"; export CMAKE_LIBRARY_PATH

AXMOL_HOME=/workspace/axmol-release-2.x; export AXMOL_HOME
AX_ROOT=/workspace/axmol-release-2.x; export AX_ROOT
CMAKE_BIN=/workspace/axmol-release-2.x/tools/external/cmake/bin
PATH="$CMAKE_BIN:$PATH"; export PATH
