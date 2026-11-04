// Upstream-vs-port equivalence test for beads::Processor.
// Build twice: -DUPSTREAM (unmodified eurorack/stmlib) and without (vendor/).
// Writes raw float32 stereo output to argv[1]; segment boundaries to stdout.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "beads/dsp/processor.h"

using namespace beads;

static const size_t kBlock = 12;  // Beads' block size at 48 kHz
static uint8_t* g_mem;
static Processor g_processor;
#ifdef UPSTREAM
static uint16_t g_reverb[32768];
#endif

// Input generator independent of stmlib::Random (which both DSPs consume).
static uint32_t g_lcg = 1;
static float Noise() { g_lcg = g_lcg * 22695477u + 1u; return float(int32_t(g_lcg)) / 2147483648.0f; }

struct Segment {
  const char* name;
  int blocks;
  float time, size, shape, pitch, density, feedback, dry_wet;
  float att_time, att_size, att_shape, att_pitch;
  SeedButtonState seed;
  bool clock;         // 1 ms pulses every `clock_period` samples on the SEED input
  int clock_period;
  bool freeze;
  int mono;           // -1: unchanged, 0/1: Configure(MEDIA_DIGITAL, mono)
};

static void Configure(bool mono) {
  g_processor.Configure(Processor::MEDIA_DIGITAL, mono);
#ifndef UPSTREAM
  // The port's Reset() no longer clears the delay memory (BeetsProcessor
  // clears it incrementally); emulate the completed clear here.
  memset(g_mem, 0, 768 * 1024);
#endif
}

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  FILE* f = fopen(argv[1], "wb");
  const float gain = argc > 2 ? float(atof(argv[2])) : 1.0f;  // input level
  g_mem = static_cast<uint8_t*>(calloc(1, 768 * 1024));
  uint8_t* banks[3] = {g_mem, g_mem + 262144, g_mem + 524288};
#ifdef UPSTREAM
  g_processor.Init(banks, g_reverb);
#else
  static float pitch_tables[stmlib::kPitchRatioTablesSize];
  stmlib::InitPitchRatioTables(pitch_tables);
  g_processor.Init(banks);
#endif

  const SeedButtonState L = SEED_BUTTON_STATE_LOCKED, ON = SEED_BUTTON_STATE_ON,
                        OFF = SEED_BUTTON_STATE_OFF;
  // name blocks  time size shape pitch dens  fb   dw   aT    aS    aSh   aP    seed clk per  frz mono
  const Segment segs[] = {
    {"fill",       16000, 0.30f,0.50f,0.50f, 0.0f,0.20f,0.00f,0.50f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"regular",    16000, 0.30f,0.45f,0.50f, 0.0f,0.20f,0.30f,0.70f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"random",     16000, 0.60f,0.40f,0.30f, 7.0f,0.80f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"deadzone",    8000, 0.60f,0.40f,0.30f, 0.0f,0.50f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"att+",       16000, 0.40f,0.55f,0.60f,-5.0f,0.75f,0.40f,1.00f, 0.6f, 0.5f, 0.4f, 0.7f, L, false,0,   false,-1},
    {"att-",       16000, 0.40f,0.55f,0.60f, 3.0f,0.25f,0.40f,1.00f,-0.6f,-0.5f,-0.4f,-0.7f, L, false,0,   false,-1},
    {"reverse",    12000, 0.50f,0.20f,0.10f,12.0f,0.10f,0.50f,1.00f, 0.0f, 0.3f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"freeze",     12000, 0.50f,0.70f,0.90f, 0.0f,0.85f,0.10f,1.00f, 0.5f, 0.0f, 0.0f, 0.0f, L, false,0,   true, -1},
    {"thaw",        8000, 0.50f,0.70f,0.90f, 0.0f,0.85f,0.10f,1.00f, 0.5f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"divider",    20000, 0.30f,0.45f,0.50f, 0.0f,0.30f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, true, 6000,false,-1},
    {"probability",20000, 0.30f,0.45f,0.50f, 0.0f,0.80f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, true, 6000,false,-1},
    {"seed on",     4000, 0.30f,0.45f,0.50f, 0.0f,0.20f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f,ON, false,0,   false,-1},
    {"seed off",    4000, 0.30f,0.45f,0.50f, 0.0f,0.20f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f,OFF,false,0,   false,-1},
    {"gated clock", 8000, 0.30f,0.45f,0.50f, 0.0f,0.20f,0.20f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f,OFF,true, 3000,false,-1},
    {"delay free", 20000, 0.30f,1.00f,0.50f, 0.0f,0.30f,0.50f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"delay pitch",12000, 0.30f,1.00f,0.50f, 7.0f,0.30f,0.50f,0.80f, 0.0f,-0.5f, 0.3f, 0.0f, L, false,0,   false,-1},
    {"delay sync", 20000, 0.40f,1.00f,0.30f, 0.0f,0.35f,0.40f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, true, 6000,false,-1},
    {"delay frz",  12000, 0.70f,1.00f,0.30f, 0.0f,0.35f,0.40f,0.80f, 0.4f, 0.0f, 0.0f, 0.0f, L, true, 6000,true, -1},
    {"delay odd",  16000, 0.30f,1.00f,0.50f, 0.0f,0.70f,0.50f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"hysteresis",  8000, 0.30f,0.985f,0.50f,0.0f,0.20f,0.30f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"back gran",  12000, 0.30f,0.975f,0.50f,0.0f,0.20f,0.30f,0.80f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"mono",       16000, 0.40f,0.50f,0.50f, 0.0f,0.75f,0.30f,0.90f, 0.5f, 0.4f, 0.3f, 0.6f, L, false,0,   false, 1},
    {"mono delay", 12000, 0.40f,1.00f,0.50f, 0.0f,0.30f,0.30f,0.90f, 0.0f, 0.0f, 0.0f, 0.0f, L, false,0,   false,-1},
    {"stereo",      8000, 0.40f,0.50f,0.50f, 0.0f,0.75f,0.30f,0.90f, 0.5f, 0.4f, 0.3f, 0.6f, L, false,0,   false, 0},
  };

  FloatFrame in[kBlock], out[kBlock];
  bool freeze[kBlock];
  stmlib::GateFlags seed[kBlock];
  stmlib::GateFlags prev = stmlib::GATE_FLAG_LOW;
  uint64_t t = 0;
  float phase = 0.0f;
  for (size_t s = 0; s < sizeof(segs) / sizeof(segs[0]); ++s) {
    const Segment& g = segs[s];
    printf("%s %llu\n", g.name, (unsigned long long)t);
    if (g.mono >= 0) Configure(g.mono != 0);
    Parameters p;
    memset(&p, 0, sizeof(p));
    p.time.base = g.time;   p.time.attenuverter = g.att_time;
    p.size.base = g.size;   p.size.attenuverter = g.att_size;
    p.shape.base = g.shape; p.shape.attenuverter = g.att_shape;
    p.pitch.base = g.pitch; p.pitch.attenuverter = g.att_pitch;
#ifdef UPSTREAM
    p.time.has_cv = p.size.has_cv = p.shape.has_cv = p.pitch.has_cv = false;
    p.time.cv = p.size.cv = p.shape.cv = p.pitch.cv = 0.0f;
#endif
    p.monophonic_mode = false;
    ModulatedParameter d;
    d.base = g.density;
#ifdef UPSTREAM
    d.cv = 0.0f;
#endif
    Processor::MixParameters m;
    m.feedback = g.feedback;
    m.dry_wet = g.dry_wet;
#ifdef UPSTREAM
    m.reverb_amount = 0.0f;
    m.output_gate = false;
#endif
    for (int b = 0; b < g.blocks; ++b) {
      for (size_t i = 0; i < kBlock; ++i, ++t) {
        phase += 0.0021f + 0.0019f * sinf(float(t) * 1e-5f);
        if (phase >= 1.0f) phase -= 1.0f;
        const float tone = 0.25f * sinf(6.2831853f * phase);
        in[i].l = gain * (tone + 0.05f * Noise());
        in[i].r = gain * (0.8f * tone + 0.05f * Noise());
        freeze[i] = g.freeze;
        // Phase offset: an edge on the first synced sample makes upstream
        // DelayClock record a 1-sample interval and divide by zero in
        // DelayController (harmless on ARM without DIV_0_TRP, traps on x86).
        const bool high = g.clock && ((t + 1000) % uint64_t(g.clock_period)) < 48;
        prev = seed[i] = stmlib::ExtractGateFlags(prev, high);
      }
      if (!g.clock) prev = stmlib::GATE_FLAG_LOW;
      g_processor.Process(g.seed, g.clock ? seed : NULL, freeze, d, p, m, in, out, kBlock);
      fwrite(out, sizeof(out), 1, f);
    }
  }
  printf("end %llu\n", (unsigned long long)t);
  fclose(f);
  return 0;
}
