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
  CHECK(fabsf(s_processor.debug_pitch() - 14.0f) < 0.01f, "PTCH 12.0 + full bend 2 (%f)", s_processor.debug_pitch());
  unit_pitch_bend(0x2000); unit_set_param_value(BEETS_PARAM_PITCH, 0);
  CHECK(unit_get_param_str_value(BEETS_PARAM_MODE, 6) && !strcmp(unit_get_param_str_value(BEETS_PARAM_MODE, 6), "DIGS"), "MODE string DIGS");

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

  // ---- SEED via notes (ui.cc state machine)
  CHECK(s_processor.debug_seed_locked(), "seed locked at boot");
  unit_note_on(60, 100); Run(0.05);
  CHECK(!s_processor.debug_seed_locked() && s_processor.debug_seed_pressed(), "press: unlocked + on");
  unit_note_off(60); Run(0.05);
  CHECK(!s_processor.debug_seed_locked() && !s_processor.debug_seed_pressed(), "release: off, stays unlocked");
  const double gated_off = Run(1.0);  // regular density, gate low -> no new grains
  printf("unlocked, no gate: rms %.5f\n", gated_off);
  CHECK(gated_off < regular * 0.2, "pulse train stops while SEED is released");
  unit_note_on(60, 100); Run(4.1);
  CHECK(s_processor.debug_seed_locked() && !s_processor.debug_seed_pressed(), "4 s hold re-locks");
  unit_note_off(60); Run(0.05);
  CHECK(s_processor.debug_seed_locked(), "still locked after release");

  // ---- clock: ticks make the SEED input "patched", 4 s silence unpatches
  CHECK(!s_processor.debug_clock_patched(), "no clock at start");
  for (int i = 0; i < 40; ++i) { unit_tempo_4ppqn_tick(i); Run(0.125); }  // 120 BPM 16ths
  CHECK(s_processor.debug_clock_patched(), "ticks -> patched");
  const double divided = Run(0.0);
  (void)divided;
  Run(4.1);
  CHECK(!s_processor.debug_clock_patched(), "4 s without ticks -> unpatched");

  // ---- block sizes and in-place rendering
  Run(0.2, 37); Run(0.2, 5); Run(0.2, 12); Run(0.2, 1);
  Run(0.2, 64, true);

  // ---- MODE change and reset clear incrementally, dry passes through
  unit_set_param_value(BEETS_PARAM_MODE, 7); Run(0.0, 64);
  FillInput(64); unit_render(in_buf, out_buf, 64);
  CHECK(s_processor.debug_mode() == 7 && s_processor.debug_clearing(), "MODE 7 -> mono + clearing");
  int calls = 1;
  while (s_processor.debug_clearing()) {
    FillInput(64); unit_render(in_buf, out_buf, 64); ++calls;
    for (int i = 0; i < 128; ++i) CHECK(out_buf[i] == in_buf[i] || !s_processor.debug_clearing(), "dry during clear");
  }
  printf("clear after MODE change took %d render calls\n", calls);
  CHECK(calls == 48, "768 KB / 16 KB = 48 calls (%d)", calls);
  unit_reset(); Run(0.01);
  CHECK(s_processor.debug_clearing(), "reset -> clearing");
  Run(0.1);
  CHECK(!s_processor.debug_clearing(), "clear finished");

  unit_teardown();
  printf(g_fail ? "%d FAILURES\n" : "all unit tests passed\n", g_fail);
  return g_fail != 0;
}
