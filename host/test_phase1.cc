#include "host_sdk.h"
#include "unit_delfx.h"
extern "C" const unit_header_t unit_header;
#include "beads_processor.h"
#include <stdio.h>
#include <math.h>
static BeetsProcessor p;
int main() {
  unit_runtime_desc_t d = {};
  d.target = unit_header.target; d.api = UNIT_API_VERSION; d.samplerate = 48000;
  d.frames_per_buffer = 64; d.input_channels = 2; d.output_channels = 2;
  d.hooks.sdram_alloc = host_sdram_alloc; d.hooks.sdram_free = host_sdram_free;
  if (p.Init(&d)) return 1;
  float in[128], out[128]; long n = 0; long first = -1;
  for (int b = 0; b < 1000; ++b) {
    for (int i = 0; i < 64; ++i) { float v = (n + i == 100) ? 0.5f : 0.f; in[2*i] = v; in[2*i+1] = -v; }
    p.Render(in, out, 64);
    for (int i = 0; i < 64; ++i) if (n + i > 100 && first < 0 && fabsf(out[2*i]) > 0.1f) { first = n + i;
      printf("echo at %ld (delay %ld) L=%f R=%f\n", first, first - 100, out[2*i], out[2*i+1]); }
    n += 64;
  }
  // Upstream Read<HERMITE>(d, 0) on a just-written line returns the sample d-1 ago.
  return first - 100 == BeetsProcessor::kFixedDelay - 1 ? 0 : 2;
}
