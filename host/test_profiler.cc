#define BEETS_PROFILE
#include "host_sdk.h"
#include "unit_delfx.h"
#include "profiler.h"
#include <stdio.h>
#include <string.h>
uint32_t g_host_fake_cycles = 0xfff00000u;  // starts near wrap on purpose
static Profiler pr;
static int fails = 0;
static void expect(int idx, const char* want) {
  const char* got = pr.Format(idx);
  if (strcmp(got, want)) { printf("diag %d: got '%s' want '%s'\n", idx, got, want); ++fails; }
}
int main() {
  pr.Init(64);
  // 550 MHz core, 64-frame buffers: period = 550e6 * 64 / 48000 = 733333 cycles.
  const uint32_t period = 733333, render = 128000;  // 2000 cycles/sample
  for (int call = 0; call < 3000; ++call) {
    pr.BeginRender(64);
    if (call % 94 == 10) { g_host_fake_cycles += 5000; pr.OnTick(); }  // tick inside period
    pr.OnDspTickInterval(call % 94 == 11, 64);
    g_host_fake_cycles += render - (call % 94 == 10 ? 5000 : 0);
    pr.EndRender(true, 64 * 4);
    g_host_fake_cycles += period - render;
  }
  expect(DIAG_RENDER_CPS_AVG, "2000");
  expect(DIAG_RENDER_CPS_PEAK, "2000");
  expect(DIAG_PERIOD_CPS_AVG, "11458");
  expect(DIAG_CORE_MHZ, "550.0");
  expect(DIAG_LOAD_AVG, "17.45");
  expect(DIAG_FRAMES_PER_BUFFER, "64");
  expect(DIAG_TICK_COUNT, "32");
  // ticks every 94 calls = 94*64 samples = 125.33 ms at 48 kHz
  expect(DIAG_TICK_MS_MIN, "125.3");
  expect(DIAG_TICK_SPREAD_US, "0");
  expect(DIAG_TICK_SMP_MIN, "6016");
  expect(DIAG_TICK_SMP_MAX, "6016");
  expect(DIAG_GRAIN_CYCLES, "0");  // x constant -> slope undefined -> 0
  expect(DIAG_RESET_PEAKS, "RST");
  return fails;
}
