// Bit-exact check of the rebuilt pitch tables against upstream stmlib.
#include <stdio.h>
#include <string.h>
#include "stmlib/dsp/units.h"
namespace ref {
#define lut_pitch_ratio_high ref_high
#define lut_pitch_ratio_low ref_low
#include "ref/units_tables_only.inc"
#undef lut_pitch_ratio_high
#undef lut_pitch_ratio_low
}
int main() {
  static float storage[stmlib::kPitchRatioTablesSize];
  stmlib::InitPitchRatioTables(storage);
  int bad = 0;
  for (int i = 0; i < 257; ++i) {
    if (memcmp(&stmlib::lut_pitch_ratio_high[i], &ref::ref_high[i], 4)) { printf("high %d\n", i); ++bad; }
    if (memcmp(&stmlib::lut_pitch_ratio_low[i], &ref::ref_low[i], 4)) { printf("low %d\n", i); ++bad; }
  }
  printf("pitch tables: %d mismatches of 514\n", bad);
  return bad != 0;
}
