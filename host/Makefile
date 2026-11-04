# Native (x86) correctness tests. Not a performance reference.
#   make -C host LOGUE_SDK=/path/to/logue-sdk
LOGUE_SDK ?= ../../logue-sdk
COMMON := $(LOGUE_SDK)/platform/nts-1_mkii/common
OUT ?= out
CXX ?= g++
CXXFLAGS := -std=c++11 -O2 -g -DTEST -Wall -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function
INC := -Istub -I. -I../src -I../vendor -I$(COMMON)
VENDOR_SRC := $(wildcard ../vendor/beads/dsp/*.cc) ../vendor/stmlib/dsp/units.cc ../vendor/stmlib/utils/random.cc

TESTS := test_phase1 test_profiler

tests: $(addprefix $(OUT)/,$(TESTS))
	@for t in $^; do echo "== $$t"; $$t || exit 1; done

$(OUT)/%: %.cc ../src/header.c $(wildcard ../src/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(INC) $< -x c++ ../src/header.c -o $@

clean:
	rm -rf $(OUT)
