#!/bin/sh
# Clean build + install. Extra args are passed to make, e.g.
#   ./build.sh BEETS_DEFS="-DBEETS_PROFILE -DBEETS_NUM_GRAINS=8"
# GCC_BIN_PATH defaults to the SDK toolchain location; override for a system toolchain.
set -e
cd "$(dirname "$0")"
make clean >/dev/null
make "$@"
make install "$@"
