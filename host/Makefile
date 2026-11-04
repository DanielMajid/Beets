# Native (x86) correctness tests. Not a performance reference.
#   make -C host LOGUE_SDK=/path/to/logue-sdk
LOGUE_SDK ?= ../../logue-sdk
COMMON := $(LOGUE_SDK)/platform/nts-1_mkii/common
OUT ?= out
CXX ?= g++
CXXFLAGS := -std=c++11 -O2 -g -DTEST -Wall -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function
INC := -Istub -I. -I../src -I../vendor -I$(COMMON)
VENDOR_SRC := $(wildcard ../vendor/beads/dsp/*.cc) ../vendor/stmlib/dsp/units.cc ../vendor/stmlib/utils/random.cc

TESTS := test_profiler test_units test_unit test_unit_mono

tests: $(addprefix $(OUT)/,$(TESTS))
	@for t in $^; do echo "== $$t"; $$t || exit 1; done

PORT_DSP := ../vendor/beads/dsp/processor.cc ../vendor/beads/dsp/trigger_generator.cc \
            ../vendor/beads/dsp/delay_controller.cc ../vendor/beads/dsp/delay_clock.cc \
            ../vendor/stmlib/dsp/units.cc ../vendor/stmlib/utils/random.cc

$(OUT)/test_units: test_units.cc ../vendor/stmlib/dsp/units.cc
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -I. -I../vendor $^ -o $@

$(OUT)/test_unit: test_unit.cc ../src/header.c $(wildcard ../src/*.h) $(PORT_DSP)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(INC) test_unit.cc $(PORT_DSP) -x c++ ../src/header.c -o $@

# Same tests with DIGM compiled back in (BEETS_ENABLE_MONO=1).
$(OUT)/test_unit_mono: test_unit.cc ../src/header.c $(wildcard ../src/*.h) $(PORT_DSP)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -DBEETS_ENABLE_MONO=1 $(INC) test_unit.cc $(PORT_DSP) -x c++ ../src/header.c -o $@

$(OUT)/%: %.cc ../src/header.c $(wildcard ../src/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(INC) $< -x c++ ../src/header.c -o $@

# ---- Upstream equivalence (needs the original trees) -------------------------
#   make fidelity EURORACK=/path/to/eurorack STMLIB=/path/to/stmlib
EURORACK ?= ../../eurorack
STMLIB ?= ../../stmlib
UP := $(EURORACK)/beads
UP_DSP := $(UP)/dsp/processor.cc $(UP)/dsp/trigger_generator.cc $(UP)/dsp/delay_controller.cc \
          $(UP)/dsp/delay_clock.cc $(UP)/dsp/mu_law.cc $(UP)/resources.cc \
          $(STMLIB)/dsp/units.cc $(STMLIB)/utils/random.cc
FID := -DBEETS_NUM_GRAINS=32 -DBEETS_ENABLE_MONO=1   # upstream grain count, DIGM in

$(OUT)/upinc:
	@mkdir -p $@
	ln -sfn $(abspath $(UP)) $@/beads
	ln -sfn $(abspath $(STMLIB)) $@/stmlib

fidelity: $(OUT)/upinc
	$(CXX) $(CXXFLAGS) -w -DUPSTREAM -Iref/identity_reverb -I$(OUT)/upinc test_fidelity.cc $(UP_DSP) -o $(OUT)/fid_up_norev
	$(CXX) $(CXXFLAGS) -w -DUPSTREAM -I$(OUT)/upinc test_fidelity.cc $(UP_DSP) -o $(OUT)/fid_up
	$(CXX) $(CXXFLAGS) -w $(FID) $(INC) test_fidelity.cc $(PORT_DSP) -o $(OUT)/fid_port
	$(CXX) $(CXXFLAGS) -w -DBEETS_NUM_GRAINS=32 $(INC) test_fidelity.cc $(PORT_DSP) -o $(OUT)/fid_port_digs
	$(CXX) $(CXXFLAGS) -w $(FID) -DHOST_FX_SINF_UPSTREAM_LUT $(INC) -I$(OUT)/upinc test_fidelity.cc $(PORT_DSP) $(UP)/resources.cc -o $(OUT)/fid_port_lut
	@for b in fid_up_norev fid_up fid_port fid_port_lut fid_port_digs; do $(OUT)/$$b $(OUT)/$$b.raw 0.5 > $(OUT)/segs.txt || exit 1; done
	@$(OUT)/fid_port $(OUT)/fid_port_edge.raw 0.5 0 > /dev/null && echo "== port with clock edges on segment starts (upstream divides by zero here): no trap"
	@echo "== [A] upstream minus reverb  vs  port using upstream's sine table (expect ALL BIT-EXACT)"
	@python3 compare.py $(OUT)/fid_up_norev.raw $(OUT)/fid_port_lut.raw $(OUT)/segs.txt
	@echo "== [B] upstream minus reverb  vs  shipping port (fx_sinf)"
	@python3 compare.py $(OUT)/fid_up_norev.raw $(OUT)/fid_port.raw $(OUT)/segs.txt
	@echo "== [C] full upstream (reverb amount 0)  vs  shipping port"
	@python3 compare.py $(OUT)/fid_up.raw $(OUT)/fid_port.raw $(OUT)/segs.txt | tail -1
	@echo "== [D] DIGS-only build vs DIGM-enabled build, stereo segments (expect ALL BIT-EXACT)"
	@python3 compare.py $(OUT)/fid_port.raw $(OUT)/fid_port_digs.raw $(OUT)/segs.txt mono | tail -1

.PHONY: tests fidelity clean

clean:
	rm -rf $(OUT)
