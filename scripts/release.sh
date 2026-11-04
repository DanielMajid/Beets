#!/bin/sh
# Builds the shipping unit and the measurement variants into release/.
# Pass the same make arguments as build.sh (LOGUE_SDK=..., GCC_BIN_PATH=...).
#
#  file                      name         defines
#  beets.nts1mkiiunit        Beets        shipping default (4 grains, DIGS, state in RAM)
#  beets-p4.nts1mkiiunit     Beets P4     profiler, 4 grains, state in RAM
#  beets-p8.nts1mkiiunit     Beets P8     profiler, 8 grains, state in RAM
#  beets-pm8.nts1mkiiunit    Beets PM8    profiler, 8 grains, DSP state in SDRAM
#  beets-pm16.nts1mkiiunit   Beets PM16   profiler, 16 grains, DSP state in SDRAM
#  beets-pm32.nts1mkiiunit   Beets PM32   profiler, 32 grains, DSP state in SDRAM
#  beets-pmx32.nts1mkiiunit  Beets PMX32  as PM32 + WasteCPUCycles stress test
set -e
cd "$(dirname "$0")/.."
mkdir -p release
build() {
  out=$1; shift
  ./build.sh "$@" >/dev/null
  scripts/audit.sh >/dev/null || { scripts/audit.sh; exit 1; }
  size=$(scripts/audit.sh | head -1)
  cp beets.nts1mkiiunit "release/$out"
  printf '%-26s %s\n' "$out" "$size"
}
build beets.nts1mkiiunit       "$@" BEETS_DEFS=""
build beets-p4.nts1mkiiunit    "$@" BEETS_DEFS="-DBEETS_VARIANT=1 -DBEETS_PROFILE -DBEETS_NUM_GRAINS=4"
build beets-p8.nts1mkiiunit    "$@" BEETS_DEFS="-DBEETS_VARIANT=2 -DBEETS_PROFILE -DBEETS_NUM_GRAINS=8"
build beets-pm8.nts1mkiiunit   "$@" BEETS_DEFS="-DBEETS_VARIANT=3 -DBEETS_PROFILE -DBEETS_STATE_IN_SDRAM -DBEETS_NUM_GRAINS=8"
build beets-pm16.nts1mkiiunit  "$@" BEETS_DEFS="-DBEETS_VARIANT=4 -DBEETS_PROFILE -DBEETS_STATE_IN_SDRAM -DBEETS_NUM_GRAINS=16"
build beets-pm32.nts1mkiiunit  "$@" BEETS_DEFS="-DBEETS_VARIANT=5 -DBEETS_PROFILE -DBEETS_STATE_IN_SDRAM -DBEETS_NUM_GRAINS=32"
build beets-pmx32.nts1mkiiunit "$@" BEETS_DEFS="-DBEETS_VARIANT=6 -DBEETS_PROFILE -DBEETS_STATE_IN_SDRAM -DBEETS_STRESS_TEST -DBEETS_NUM_GRAINS=32"
./build.sh "$@" BEETS_DEFS="" >/dev/null  # leave the default build in place
