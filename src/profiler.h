/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: profiler.h
 *
 * DWT cycle-counter instrumentation. Compiled in only with -DBEETS_PROFILE;
 * otherwise every method is an empty inline and costs nothing.
 *
 * What it measures (all wrap-safe uint32 differences of DWT->CYCCNT):
 *   - cycles inside unit_render, per sample (avg + peak)
 *   - cycles between the starts of consecutive unit_render calls, per sample
 *     (= core clock / 48000; also how much of the period everybody else gets)
 *   - frames_per_buffer and the actual frames argument (min/max)
 *   - unit_tempo_4ppqn_tick interval in wall-clock time (min/max) and as seen
 *     by the DSP in samples (block-quantised, min/max), plus where inside the
 *     audio period the tick lands (tells synchronous from asynchronous delivery)
 *   - a one-shot SDRAM read probe: sequential vs random, cycles per read
 *   - counts of note-on / pitch-bend callbacks (do they reach a delfx?)
 *   - cycles per active grain: least-squares slope of render cycles against
 *     the number of active grains, accumulated over many blocks
 *
 * CYCCNT is enabled but never zeroed: the firmware may rely on it.
 *
 * Readout: the DIAG parameter (slot 10, replacing FRZE, in profiling builds). Each value of
 * DIAG selects one figure; the string shown is that figure. The display
 * refreshes when the value is re-selected. See NOTES.md for the index table.
 */

#ifndef BEETS_PROFILER_H_
#define BEETS_PROFILER_H_

#include <stdint.h>
#include <stddef.h>

#include "beets_config.h"

#if defined(BEETS_PROFILE) && !defined(TEST)
#define BEETS_PROFILER_ACTIVE 1
// ARMv7-M architectural addresses (ARM DDI 0403, C1.6 / C1.8). Addressed
// directly because the SDK pulls core_cm7.h through arm_math.h with
// __CMSIS_GENERIC, which leaves DWT and CoreDebug undeclared.
#define BEETS_DWT_CTRL   (*(volatile uint32_t*)0xE0001000u)
#define BEETS_DWT_CYCCNT (*(volatile uint32_t*)0xE0001004u)
#define BEETS_DWT_LAR    (*(volatile uint32_t*)0xE0001FB0u)
#define BEETS_DEMCR      (*(volatile uint32_t*)0xE000EDFCu)
#define BEETS_SCB_CCR    (*(volatile uint32_t*)0xE000ED14u)
#define BEETS_DEMCR_TRCENA     (1u << 24)
#define BEETS_DWT_CYCCNTENA    (1u << 0)
static inline uint32_t BeetsCycles() { return BEETS_DWT_CYCCNT; }
#elif defined(BEETS_PROFILE)
#define BEETS_PROFILER_ACTIVE 1
// Host build: deterministic fake counter so the bookkeeping can be tested.
extern uint32_t g_host_fake_cycles;
static inline uint32_t BeetsCycles() { return g_host_fake_cycles; }
#endif

enum DiagIndex {
  DIAG_RENDER_CPS_AVG = 0,   // render cycles per sample, rolling average
  DIAG_RENDER_CPS_PEAK,      // render cycles per sample, peak
  DIAG_PERIOD_CPS_AVG,       // audio period cycles per sample (avg)
  DIAG_PERIOD_CPS_PEAK,      // audio period cycles per sample (peak)
  DIAG_CORE_MHZ,             // core clock derived from the period
  DIAG_LOAD_AVG,             // render / period, percent
  DIAG_LOAD_PEAK,
  DIAG_FRAMES_PER_BUFFER,    // desc->frames_per_buffer
  DIAG_FRAMES_MIN,           // smallest frames argument seen
  DIAG_FRAMES_MAX,           // largest frames argument seen
  DIAG_TICK_COUNT,           // 4PPQN ticks received (mod 10000)
  DIAG_TICK_MS_MIN,          // tick interval, wall clock, milliseconds
  DIAG_TICK_MS_MAX,
  DIAG_TICK_SPREAD_US,       // max - min, microseconds
  DIAG_TICK_SMP_MIN,         // tick interval as seen by the DSP, samples
  DIAG_TICK_SMP_MAX,
  DIAG_TICK_PHASE_MIN,       // tick time inside the audio period, percent
  DIAG_TICK_PHASE_MAX,
  DIAG_SDRAM_SEQ,            // cycles per sequential 16-bit SDRAM read
  DIAG_SDRAM_RAND,           // cycles per random 16-bit SDRAM read (768 KB span)
  DIAG_SDRAM_HOT,            // cycles per random read inside 4 KB (cache-resident)
  DIAG_SDRAM_RATIO,          // RAND / SEQ
  DIAG_SDRAM_QUAD,           // cycles per 4-sample Hermite-style read at random
  DIAG_GRAIN_CYCLES,         // cycles per active grain per sample (slope)
  DIAG_GRAINS_PEAK,          // peak number of active grains
  DIAG_NOTE_ON_COUNT,        // unit_note_on calls received
  DIAG_PITCH_BEND_COUNT,     // unit_pitch_bend calls received
  DIAG_CCR_TRAPS,            // SCB->CCR: 10 x DIV_0_TRP + UNALIGN_TRP (0, 1, 10, 11)
  DIAG_RESET_PEAKS,          // selecting this value resets all peaks / min / max
  DIAG_LAST
};

static_assert(DIAG_LAST == BEETS_DIAG_COUNT, "update BEETS_DIAG_COUNT");

#ifdef BEETS_PROFILER_ACTIVE

class Profiler {
 public:
  void Init(uint16_t frames_per_buffer) {
    // Enable the cycle counter (trace enable, unlock, count enable).
#ifndef TEST
    BEETS_DEMCR |= BEETS_DEMCR_TRCENA;
    BEETS_DWT_LAR = 0xC5ACCE55u;  // unlock (M7 DWT has a lock register)
    BEETS_DWT_CTRL |= BEETS_DWT_CYCCNTENA;
#endif
    frames_per_buffer_ = frames_per_buffer;
    render_cps_avg_ = period_cps_avg_ = load_avg_ = 0.0f;
    calls_ = 0;
    ResetPeaks();
    tick_count_ = 0;
    tick_last_cycles_ = 0;
    tick_have_last_ = false;
    tick_pending_ = 0;
    tick_last_cycles_in_period_ = 0;
    dsp_samples_since_tick_ = 0;
    dsp_have_tick_ = false;
    note_on_count_ = pitch_bend_count_ = 0;
    sdram_seq_ = sdram_rand_ = sdram_hot_ = sdram_quad_ = 0.0f;
    ClearGrainFit();
    last_render_start_ = 0;
    prefix_len_ = 0;
  }

  void ResetPeaks() {
    render_cps_peak_ = period_cps_peak_ = load_peak_ = 0.0f;
    frames_min_ = 0xffff;
    frames_max_ = 0;
    tick_cycles_min_ = 0xffffffffu;
    tick_cycles_max_ = 0;
    tick_smp_min_ = 0xffffffffu;
    tick_smp_max_ = 0;
    tick_phase_min_ = 1.0f;
    tick_phase_max_ = 0.0f;
    grains_peak_ = 0;
    ClearGrainFit();
  }

  // ---- render bracket ------------------------------------------------------
  inline void BeginRender(uint32_t frames) {
    const uint32_t now = BeetsCycles();
    if (calls_) {
      period_cycles_ = now - last_render_start_;
    }
    last_render_start_ = now;
    frames_ = frames;
    if (frames < frames_min_) frames_min_ = frames;
    if (frames > frames_max_) frames_max_ = frames;
  }

  // grain_samples: sum over the call's samples of the number of active
  // grains (0 if no granular engine yet); used for the per-grain cost fit.
  inline void EndRender(bool steady, uint32_t grain_samples) {
    const uint32_t render = BeetsCycles() - last_render_start_;
    if (!frames_) return;
    const float inv_frames = 1.0f / float(frames_);
    const float render_cps = float(render) * inv_frames;
    ++calls_;
    if (calls_ < 2) return;  // no period yet
    const float period_cps = float(period_cycles_) * inv_frames;
    const float load = period_cycles_ ? float(render) / float(period_cycles_) : 0.0f;
    const float k = calls_ < 64 ? 0.25f : (1.0f / 256.0f);
    render_cps_avg_ += k * (render_cps - render_cps_avg_);
    period_cps_avg_ += k * (period_cps - period_cps_avg_);
    load_avg_ += k * (load - load_avg_);
    // Peaks ignore the first second (cold caches, buffer clearing).
    if (steady && calls_ > 1000) {
      if (render_cps > render_cps_peak_) render_cps_peak_ = render_cps;
      if (period_cps > period_cps_peak_) period_cps_peak_ = period_cps;
      if (load > load_peak_) load_peak_ = load;
      // Per-grain fit: x = average active grains, y = render cycles/sample.
      const float x = float(grain_samples) * inv_frames;
      fit_n_ += 1.0f;
      fit_x_ += x;
      fit_y_ += render_cps;
      fit_xx_ += x * x;
      fit_xy_ += x * render_cps;
    }
  }

  inline void NoteGrains(uint32_t active) {
    if (active > grains_peak_) grains_peak_ = active;
  }

  // ---- 4PPQN tick ----------------------------------------------------------
  // Called from unit_tempo_4ppqn_tick: one counter read and a few compares.
  inline void OnTick() {
    const uint32_t now = BeetsCycles();
    if (tick_have_last_) {
      const uint32_t d = now - tick_last_cycles_;
      if (d < tick_cycles_min_) tick_cycles_min_ = d;
      if (d > tick_cycles_max_) tick_cycles_max_ = d;
    }
    tick_have_last_ = true;
    tick_last_cycles_ = now;
    tick_last_cycles_in_period_ = now - last_render_start_;
    ++tick_count_;
    tick_pending_ = 1;
  }

  // Called once per render with the frame count, after ticks were consumed:
  // records the interval between ticks in samples as delivered to the DSP.
  inline void OnDspTickInterval(bool tick_this_block, uint32_t frames) {
    if (tick_this_block) {
      if (dsp_have_tick_) {
        const uint32_t s = dsp_samples_since_tick_;
        if (s < tick_smp_min_) tick_smp_min_ = s;
        if (s > tick_smp_max_) tick_smp_max_ = s;
      }
      dsp_have_tick_ = true;
      dsp_samples_since_tick_ = 0;
      if (period_cycles_) {
        float ph = float(tick_last_cycles_in_period_) / float(period_cycles_);
        if (ph < tick_phase_min_) tick_phase_min_ = ph;
        if (ph > tick_phase_max_) tick_phase_max_ = ph;
      }
    }
    dsp_samples_since_tick_ += frames;
  }

  inline void OnNoteOn() { ++note_on_count_; }
  inline void OnPitchBend() { ++pitch_bend_count_; }

  // ---- SDRAM probe ---------------------------------------------------------
  // Run once from unit_init over the allocated delay memory.
  void ProbeSdram(const uint8_t* base, uint32_t size) {
    const uint32_t kReads = 16384;
    const volatile int16_t* p = reinterpret_cast<const volatile int16_t*>(base);
    // size must be a power of two: every random loop below indexes with a
    // mask, so the loops differ only in where they read, not in ALU cost.
    const uint32_t mask = size / 2 - 1;
    int32_t sink = 0;

    // Sequential.
    uint32_t t0 = BeetsCycles();
    for (uint32_t i = 0; i < kReads; ++i) sink += p[i];
    uint32_t seq = BeetsCycles() - t0;

    // Random over the whole span (same LCG cost in every random loop).
    uint32_t r = 12345;
    t0 = BeetsCycles();
    for (uint32_t i = 0; i < kReads; ++i) {
      r = r * 1664525u + 1013904223u;
      sink += p[(r >> 8) & mask];
    }
    uint32_t rnd = BeetsCycles() - t0;

    // Random inside 4 KB: same loop, cache-resident after the first pass.
    for (uint32_t i = 0; i < 2048; ++i) sink += p[i];
    t0 = BeetsCycles();
    for (uint32_t i = 0; i < kReads; ++i) {
      r = r * 1664525u + 1013904223u;
      sink += p[(r >> 8) & 2047];
    }
    uint32_t hot = BeetsCycles() - t0;

    // 4 adjacent samples at a random position (one Hermite read).
    t0 = BeetsCycles();
    for (uint32_t i = 0; i < kReads / 4; ++i) {
      r = r * 1664525u + 1013904223u;
      uint32_t j = (r >> 8) & mask;
      if (j > mask - 3) j -= 4;
      sink += p[j] + p[j + 1] + p[j + 2] + p[j + 3];
    }
    uint32_t quad = BeetsCycles() - t0;

    sink_ = sink;
    sdram_seq_ = float(seq) / float(kReads);
    sdram_rand_ = float(rnd) / float(kReads);
    sdram_hot_ = float(hot) / float(kReads);
    sdram_quad_ = float(quad) / float(kReads / 4);
  }

  // ---- readout -------------------------------------------------------------
  // The display shows only this string, so it starts with the DIAG index:
  // "05 17.45" = DIAG 5 reads 17.45. (':' is not in the display character set.)
  // Fixed width (2-digit index, value of at least 3 characters): on hardware,
  // values whose string was 4 characters or fewer ("7 64", "23 0") could not
  // be selected - the NTS-1 fell back to the previous value.
  const char* Format(int32_t index) {
    char* p = text_;
    *p++ = char('0' + (index / 10) % 10);
    *p++ = char('0' + index % 10);
    *p++ = ' ';
    prefix_len_ = int(p - text_);
    *p = 0;
    const char* value = FormatValue(index);
    if (value != text_) {  // constant strings: append after the prefix
      while (*value && p < text_ + sizeof(text_) - 1) *p++ = *value++;
      *p = 0;
    }
    return text_;
  }

 private:
  const char* FormatValue(int32_t index) {
    switch (index) {
      case DIAG_RENDER_CPS_AVG: return FormatNumber(render_cps_avg_);
      case DIAG_RENDER_CPS_PEAK: return FormatNumber(render_cps_peak_);
      case DIAG_PERIOD_CPS_AVG: return FormatNumber(period_cps_avg_);
      case DIAG_PERIOD_CPS_PEAK: return FormatNumber(period_cps_peak_);
      case DIAG_CORE_MHZ: return FormatNumber(period_cps_avg_ * 0.048f);
      case DIAG_LOAD_AVG: return FormatNumber(load_avg_ * 100.0f);
      case DIAG_LOAD_PEAK: return FormatNumber(load_peak_ * 100.0f);
      case DIAG_FRAMES_PER_BUFFER: return FormatNumber(float(frames_per_buffer_));
      case DIAG_FRAMES_MIN: return FormatNumber(frames_max_ ? float(frames_min_) : 0.0f);
      case DIAG_FRAMES_MAX: return FormatNumber(float(frames_max_));
      case DIAG_TICK_COUNT: return FormatNumber(float(tick_count_ % 10000));
      case DIAG_TICK_MS_MIN: return FormatNumber(CyclesToUs(tick_cycles_min_, true) * 0.001f);
      case DIAG_TICK_MS_MAX: return FormatNumber(CyclesToUs(tick_cycles_max_, false) * 0.001f);
      case DIAG_TICK_SPREAD_US:
        return FormatNumber(tick_cycles_max_ >= tick_cycles_min_
            ? CyclesToUs(tick_cycles_max_ - tick_cycles_min_, false) : 0.0f);
      case DIAG_TICK_SMP_MIN:
        return FormatNumber(tick_smp_min_ == 0xffffffffu ? 0.0f : float(tick_smp_min_));
      case DIAG_TICK_SMP_MAX: return FormatNumber(float(tick_smp_max_));
      case DIAG_TICK_PHASE_MIN:
        return FormatNumber(tick_phase_min_ > tick_phase_max_ ? 0.0f : tick_phase_min_ * 100.0f);
      case DIAG_TICK_PHASE_MAX: return FormatNumber(tick_phase_max_ * 100.0f);
      case DIAG_SDRAM_SEQ: return FormatNumber(sdram_seq_);
      case DIAG_SDRAM_RAND: return FormatNumber(sdram_rand_);
      case DIAG_SDRAM_HOT: return FormatNumber(sdram_hot_);
      case DIAG_SDRAM_RATIO:
        return FormatNumber(sdram_seq_ > 0.0f ? sdram_rand_ / sdram_seq_ : 0.0f);
      case DIAG_SDRAM_QUAD: return FormatNumber(sdram_quad_);
      case DIAG_GRAIN_CYCLES: return FormatNumber(GrainSlope());
      case DIAG_GRAINS_PEAK: return FormatNumber(float(grains_peak_));
      case DIAG_NOTE_ON_COUNT: return FormatNumber(float(note_on_count_ % 10000));
      case DIAG_PITCH_BEND_COUNT: return FormatNumber(float(pitch_bend_count_ % 10000));
      case DIAG_CCR_TRAPS: {
#ifndef TEST
        const uint32_t ccr = BEETS_SCB_CCR;
#else
        const uint32_t ccr = 0;
#endif
        return FormatNumber(float(((ccr >> 4) & 1) * 10 + ((ccr >> 3) & 1)));
      }
      case DIAG_RESET_PEAKS: return "RST";
      default: return "";
    }
  }

 public:
  void Select(int32_t index) {
    if (index == DIAG_RESET_PEAKS) reset_request_ = true;
  }

  // Peak reset is applied from the audio context.
  inline void ServiceRequests() {
    if (reset_request_) {
      reset_request_ = false;
      ResetPeaks();
    }
  }

  inline bool tick_pending() const { return tick_pending_ != 0; }

 private:
  void ClearGrainFit() {
    fit_n_ = fit_x_ = fit_y_ = fit_xx_ = fit_xy_ = 0.0f;
  }

  float GrainSlope() const {
    const float d = fit_n_ * fit_xx_ - fit_x_ * fit_x_;
    if (fit_n_ < 2.0f || d <= 1e-6f) return 0.0f;
    return (fit_n_ * fit_xy_ - fit_x_ * fit_y_) / d;
  }

  float CyclesToUs(uint32_t cycles, bool is_min) const {
    if (is_min && cycles == 0xffffffffu) return 0.0f;
    const float mhz = period_cps_avg_ * 0.048f;
    return mhz > 0.0f ? float(cycles) / mhz : 0.0f;
  }

  // Integers up to 5 digits ("11458"), else 4 significant digits ("12.34",
  // "1.234"), "123k" above 99999. The device display may truncate long strings.
  // Writes after the "N " prefix that Format() put in text_.
  const char* FormatNumber(float v) {
    char* p = text_ + prefix_len_;
    if (v < 0.0f) { *p++ = '-'; v = -v; }
    uint32_t scaled;
    int decimals;
    char suffix = 0;
    if (v >= 99999.5f) {
      v *= 0.001f;
      suffix = 'k';
      if (v >= 99999.5f) { v = 99999.0f; }
    }
    const bool integral = v == float(uint32_t(v));
    if (v >= 999.95f || suffix || integral) { scaled = uint32_t(v + 0.5f); decimals = 0; }
    else if (v >= 99.995f) { scaled = uint32_t(v * 10.0f + 0.5f); decimals = 1; }
    else if (v >= 9.9995f) { scaled = uint32_t(v * 100.0f + 0.5f); decimals = 2; }
    else { scaled = uint32_t(v * 1000.0f + 0.5f); decimals = 3; }
    char digits[12];
    int n = 0;
    do { digits[n++] = char('0' + scaled % 10); scaled /= 10; } while (scaled);
    while (n <= decimals) digits[n++] = '0';
    while (n < 3) digits[n++] = '0';  // at least 3 characters: "064", "000"
    for (int i = n - 1; i >= 0; --i) {
      *p++ = digits[i];
      if (i == decimals && decimals) *p++ = '.';
    }
    if (suffix) *p++ = suffix;
    *p = 0;
    return text_;
  }

  uint16_t frames_per_buffer_;
  uint32_t frames_;
  uint32_t calls_;
  uint32_t last_render_start_;
  uint32_t period_cycles_;

  float render_cps_avg_, render_cps_peak_;
  float period_cps_avg_, period_cps_peak_;
  float load_avg_, load_peak_;
  uint32_t frames_min_, frames_max_;

  volatile uint32_t tick_count_;
  volatile uint32_t tick_last_cycles_;
  volatile uint32_t tick_cycles_min_, tick_cycles_max_;
  volatile uint32_t tick_last_cycles_in_period_;
  volatile uint32_t tick_pending_;
  volatile bool tick_have_last_;
  uint32_t dsp_samples_since_tick_;
  bool dsp_have_tick_;
  uint32_t tick_smp_min_, tick_smp_max_;
  float tick_phase_min_, tick_phase_max_;

  volatile uint32_t note_on_count_, pitch_bend_count_;

  float sdram_seq_, sdram_rand_, sdram_hot_, sdram_quad_;
  int32_t sink_;

  float fit_n_, fit_x_, fit_y_, fit_xx_, fit_xy_;
  uint32_t grains_peak_;

  volatile bool reset_request_;
  char text_[16];
  int prefix_len_;
};

#else  // !BEETS_PROFILER_ACTIVE

class Profiler {
 public:
  inline void Init(uint16_t) {}
  inline void BeginRender(uint32_t) {}
  inline void EndRender(bool, uint32_t) {}
  inline void NoteGrains(uint32_t) {}
  inline void OnTick() {}
  inline void OnDspTickInterval(bool, uint32_t) {}
  inline void OnNoteOn() {}
  inline void OnPitchBend() {}
  inline void ProbeSdram(const uint8_t*, uint32_t) {}
  inline const char* Format(int32_t) { return ""; }
  inline void Select(int32_t) {}
  inline void ServiceRequests() {}
};

#endif  // BEETS_PROFILER_ACTIVE

#endif  // BEETS_PROFILER_H_
