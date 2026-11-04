// Host-only replacement for logue-sdk common/unit_delfx.h + fx_api.h subset.
// Lets the unit's DSP compile and run natively for correctness tests.
#pragma once
#include <stdint.h>
#include <math.h>
#include "runtime.h"
#include "unit.h"
#include "utils/float_math.h"
#define UNIT_DELFX_MAX_PARAM_COUNT (11)
#ifndef fast_inline
#define fast_inline inline __attribute__((always_inline))
#endif
#define k_wt_sine_size_exp (7)
#define k_wt_sine_size (1U << k_wt_sine_size_exp)
#define k_wt_sine_mask (k_wt_sine_size - 1)
// Same content as the firmware table: half sine period, 129 points.
struct HostSineLut {
  float v[k_wt_sine_size + 1];
  HostSineLut() { for (unsigned i = 0; i <= k_wt_sine_size; ++i) v[i] = (float)sin(M_PI * i / k_wt_sine_size); }
};
static HostSineLut host_sine_lut;
static inline float fx_sinf(float x) {  // Copied algorithm from fx_api.h
  const float p = x - (uint32_t)x;
  const float x0f = 2.f * p * k_wt_sine_size;
  const uint32_t x0p = (uint32_t)x0f;
  const uint32_t x0 = x0p & k_wt_sine_mask;
  const uint32_t x1 = (x0 + 1) & k_wt_sine_mask;
  const float y0 = linintf(x0f - x0p, host_sine_lut.v[x0], host_sine_lut.v[x1]);
  return (x0p < k_wt_sine_size) ? y0 : -y0;
}
