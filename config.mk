##############################################################################
# Configuration for Makefile
#

PROJECT := beets
PROJECT_TYPE := delfx

##############################################################################
# Sources
#

# C sources
UCSRC = src/header.c src/mem.c

# C++ sources
UCXXSRC = src/unit.cc \
          vendor/beads/dsp/processor.cc \
          vendor/beads/dsp/trigger_generator.cc \
          vendor/beads/dsp/delay_controller.cc \
          vendor/beads/dsp/delay_clock.cc \
          vendor/stmlib/dsp/units.cc \
          vendor/stmlib/utils/random.cc

# List ASM source files here
UASMSRC =

UASMXSRC =

##############################################################################
# Include Paths
#

UINCDIR = src vendor

##############################################################################
# Library Paths
#

ULIBDIR =

##############################################################################
# Libraries
#

ULIBS  = -lm
# Export only the unit API (see ld/exports.ver): saves ~8 KB of dynamic
# symbol tables and PLT stubs.
ULIBS += -Wl,--version-script=$(PROJECT_ROOT)/ld/exports.ver

##############################################################################
# Macros
#

# _DEFAULT_SOURCE: newlib hides M_PI under -std=c++11 (__STRICT_ANSI__);
# stmlib/dsp/filter.h needs it.
# Build variants go in BEETS_DEFS (UDEFS on the make command line would
# replace this line), e.g.:
#   ./build.sh BEETS_DEFS="-DBEETS_PROFILE -DBEETS_NUM_GRAINS=8"
UDEFS = -D_DEFAULT_SOURCE $(BEETS_DEFS)
