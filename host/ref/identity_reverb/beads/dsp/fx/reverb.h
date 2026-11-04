// Test-only stand-in for upstream beads/dsp/fx/reverb.h: an exact bypass.
// Lets the upstream reference be "Beads with the reverb removed" without
// editing any upstream file (found first on the include path).
#ifndef BEADS_DSP_FX_REVERB_H_
#define BEADS_DSP_FX_REVERB_H_
#include "beads/dsp/frame.h"
namespace beads {
class Reverb {
 public:
  void Init(uint16_t*, float) {}
  void Reset(float) {}
  void set_mix(float) {}
  void set_time(float) {}
  void set_lp(float) {}
  void Process(FloatFrame*, size_t) {}
};
}  // namespace beads
#endif
