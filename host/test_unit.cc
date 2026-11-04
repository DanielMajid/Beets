// Adaptation-layer tests through the real unit API (src/unit.cc).
#include <stdio.h>
#include <math.h>
#include "host_sdk.h"
#include "../src/unit.cc"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); ++g_fail; } } while (0)

static unit_runtime_desc_t Desc() {
  unit_runtime_desc_t d = {};
  d.target = unit_header.target; d.api = UNIT_API_VERSION; d.samplerate = 48000;
  d.frames_per_buffer = 64; d.input_channels = 2; d.output_channels = 2;
  d.hooks.sdram_alloc = host_sdram_alloc; d.hooks.sdram_free = host_sdram_free;
  d.hooks.sdram_avail = host_sdram_avail;
  return d;
}
static uint8_t* null_alloc(size_t) { return NULL; }

static float in_buf[2 * 64], out_buf[2 * 64];
static uint32_t g_n = 0;
static void FillInput(uint32_t frames) {
  for (uint32_t i = 0; i < frames; ++i, ++g_n) {
    const float v = 0.3f * sinf(0.02f * float(g_n));
    in_buf[2 * i] = v; in_buf[2 * i + 1] = -v;
  }
}
// Renders `seconds` of audio in 64-frame buffers, returns rms of output.
static double Run(double seconds, uint32_t frames = 64, bool in_place = false) {
  double acc = 0; uint64_t n = 0;
  const uint64_t total = uint64_t(seconds * 48000);
  for (uint64_t done = 0; done < total; done += frames) {
    FillInput(frames);
    if (in_place) { for (uint32_t i = 0; i < 2 * frames; ++i) out_buf[i] = in_buf[i]; unit_render(out_buf, out_buf, frames); }
    else unit_render(in_buf, out_buf, frames);
    for (uint32_t i = 0; i < 2 * frames; ++i) {
      CHECK(std::isfinite(out_buf[i]), "non-finite output");
      acc += double(out_buf[i]) * out_buf[i]; ++n;
    }
  }
  return sqrt(acc / double(n));
}

int main() {
  // ---- runtime checks
  unit_runtime_desc_t d = Desc();
  d.samplerate = 44100; CHECK(unit_init(&d) == k_unit_err_samplerate, "samplerate");
  d = Desc(); d.input_channels = 1; CHECK(unit_init(&d) == k_unit_err_geometry, "geometry");
  d = Desc(); d.hooks.sdram_alloc = null_alloc; CHECK(unit_init(&d) == k_unit_err_memory, "memory");
  d = Desc(); CHECK(unit_init(&d) == k_unit_err_none, "init");

  // ---- parameter mapping and clipping
  unit_set_param_value(BEETS_PARAM_SIZE, 5000);
  CHECK(unit_get_param_value(BEETS_PARAM_SIZE) == 1023, "SIZE clipped to max");
  Run(0.1);
  CHECK(s_processor.debug_size() > 0.99f, "SIZE max -> above the 0.99 delay-mode threshold (%f)", s_processor.debug_size());
  unit_set_param_value(BEETS_PARAM_SIZE, 1002); Run(0.1);
  CHECK(s_processor.debug_size() < 0.98f, "SIZE 1002 below the 0.98 exit threshold (%f)", s_processor.debug_size());
  unit_set_param_value(BEETS_PARAM_SIZE, 512);
  unit_set_param_value(BEETS_PARAM_DENSITY, 0); Run(0.1);
  CHECK(s_processor.debug_density() > 0.48f && s_processor.debug_density() < 0.52f, "DENS 0 inside dead zone (%f)", s_processor.debug_density());
  unit_set_param_value(BEETS_PARAM_RAND, 5); Run(0.1);
  CHECK(fabsf(s_processor.debug_attenuverter_rand()) < 1e-6f, "attenuverter dead zone (+-3%%) (%f)", s_processor.debug_attenuverter_rand());
  unit_set_param_value(BEETS_PARAM_RAND, 100); Run(0.1);
  CHECK(fabsf(s_processor.debug_attenuverter_rand() - 1.0f) < 1e-3f, "attenuverter full scale (%f)", s_processor.debug_attenuverter_rand());
  unit_set_param_value(BEETS_PARAM_RAND, -100); Run(0.1);
  CHECK(fabsf(s_processor.debug_attenuverter_rand() + 1.0f) < 1e-3f, "attenuverter -full scale");
  unit_set_param_value(BEETS_PARAM_RAND, 0);
  unit_set_param_value(BEETS_PARAM_PITCH, 120); unit_pitch_bend(0x3fff); Run(0.1);
  CHECK(fabsf(s_processor.debug_pitch() - 36.0f) < 0.01f, "PTCH 12.0 + full bend 24 (%f)", s_processor.debug_pitch());
  unit_pitch_bend(0); Run(0.1);
  CHECK(fabsf(s_processor.debug_pitch() + 12.0f) < 0.01f, "PTCH 12.0 - full bend 24 (%f)", s_processor.debug_pitch());
  unit_pitch_bend(0x2000); unit_set_param_value(BEETS_PARAM_PITCH, 0);
#if BEETS_ENABLE_MONO
  CHECK(unit_get_param_str_value(BEETS_PARAM_SYNC, 3) && !strcmp(unit_get_param_str_value(BEETS_PARAM_SYNC, 3), "SYNM"), "MODE string SYNM");
#endif

  // ---- density: centre is silence, sides produce grains (MIX fully wet)
  unit_set_param_value(BEETS_PARAM_MIX, 1000);
  unit_set_param_value(BEETS_PARAM_DENSITY, 0); Run(1.0);
  const double centre = Run(1.0);
  unit_set_param_value(BEETS_PARAM_DENSITY, -400); Run(0.5);
  const double regular = Run(1.0);
  unit_set_param_value(BEETS_PARAM_DENSITY, 400); Run(0.5);
  const double randomised = Run(1.0);
  printf("wet rms: centre %.5f, regular %.4f, random %.4f\n", centre, regular, randomised);
  CHECK(centre < 1e-3, "dead zone is silent");
  CHECK(regular > 0.02 && randomised > 0.02, "grains on both sides of centre");

  // ---- notes do not touch SEED: grains keep free-running
  unit_note_on(60, 100); Run(0.5);
  unit_note_off(60);
  const double after_note = Run(1.0);
  CHECK(after_note > regular * 0.5, "free-running grains unaffected by notes (%f)", after_note);

  // ---- SYNC: ticks reach Beads' SEED input only while SYNC is on
  for (int i = 0; i < 16; ++i) { unit_tempo_4ppqn_tick(i); Run(0.125); }
  CHECK(!s_processor.debug_clock_patched(), "SYNC off: ticks ignored");
  unit_set_param_value(BEETS_PARAM_SYNC, BEETS_ENABLE_MONO ? 2 : 1);
  CHECK(s_processor.debug_clock_patched(), "SYNC on -> clock patched");
  unit_set_param_value(BEETS_PARAM_DENSITY, -512);  // fully left: one grain per tick (/1)
  for (int i = 0; i < 32; ++i) { unit_tempo_4ppqn_tick(i); Run(0.125); }
  double synced = 0;
  for (int i = 0; i < 32; ++i) { unit_tempo_4ppqn_tick(i); synced += Run(0.125) / 32; }
  printf("SYNC on, clocked divider: rms %.4f\n", synced);
  CHECK(synced > 0.01, "clock-divided grains play");
  double stopped = 0;
  Run(4.0);
  stopped = Run(1.0);
  printf("SYNC on, clock stopped: rms %.5f\n", stopped);
  CHECK(stopped < synced * 0.2, "no clock -> no new grains (as a patched Beads)");
  // Synced delay mode: SIZE max, ticks arriving.
  unit_set_param_value(BEETS_PARAM_SIZE, 1023);
  for (int i = 0; i < 48; ++i) { unit_tempo_4ppqn_tick(i); Run(0.125); }
  unit_set_param_value(BEETS_PARAM_SIZE, 512);
  unit_set_param_value(BEETS_PARAM_SYNC, 0);
  CHECK(!s_processor.debug_clock_patched(), "SYNC off again");

  // ---- block sizes and in-place rendering
  Run(0.2, 37); Run(0.2, 5); Run(0.2, 12); Run(0.2, 1);
  Run(0.2, 64, true);

#if BEETS_ENABLE_MONO
  // ---- MODE change and reset clear incrementally, dry passes through
  unit_set_param_value(BEETS_PARAM_SYNC, 1);  // DIGM
  FillInput(64); unit_render(in_buf, out_buf, 64);
  CHECK(s_processor.debug_mono() && s_processor.debug_clearing(), "DIGM -> mono + clearing");
  int calls = 1;
  while (s_processor.debug_clearing()) {
    FillInput(64); unit_render(in_buf, out_buf, 64); ++calls;
    for (int i = 0; i < 128; ++i) CHECK(out_buf[i] == in_buf[i] || !s_processor.debug_clearing(), "dry during clear");
  }
  printf("clear after MODE change took %d render calls\n", calls);
  CHECK(calls == 48, "768 KB / 16 KB = 48 calls (%d)", calls);
  Run(2.0);  // mono granular
#endif
  unit_reset(); Run(0.01);
  CHECK(s_processor.debug_clearing(), "reset -> clearing");
  Run(0.1);
  CHECK(!s_processor.debug_clearing(), "clear finished");

  unit_teardown();
  printf(g_fail ? "%d FAILURES\n" : "all unit tests passed\n", g_fail);
  return g_fail != 0;
}
