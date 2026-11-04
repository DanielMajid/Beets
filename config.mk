##############################################################################
# Configuration for Makefile
#

PROJECT := beets
PROJECT_TYPE := delfx

##############################################################################
# Sources
#

# C sources
UCSRC = src/header.c

# C++ sources
UCXXSRC = src/unit.cc

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

##############################################################################
# Macros
#

UDEFS =
