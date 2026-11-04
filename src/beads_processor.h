/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: beads_processor.h
 *
 * The adaptation layer between the logue SDK unit API and beads::Processor.
 * Everything that replaces Beads' hardware (pots, buttons, jacks, codec
 * driver, beads.cc) lives here; the DSP itself is the vendored Beads code.
 *
 * Responsibilities:
 *  - SDRAM: two sdram_alloc calls laid out like beads.cc, incremental clear.
 *  - Control rate: the host buffer is cut into BEETS_BLOCK_SIZE (12) frame
 *    blocks = Beads' 4 kHz block rate, and controls are read once per block.
 *  - Pots: one-pole 0.05 per block plus the Beads pot laws (attenuverter dead
 *    zone curve; hysteresis 0.001 on TIME), as in beads/cv_reader.{h,cc}.
 *  - SEED button: MIDI note on/off, with the ui.cc state machine
 *    (locked by default; press = unlock + gate; 4 s hold re-locks).
 *  - SEED input jack: the 4PPQN tick, as 1 ms pulses. Counted as patched
 *    while ticks keep arriving (see BEETS_CLOCK_TIMEOUT_SAMPLES).
 *  - Parameter values arrive in a non-audio context: they are only cached
 *    there and picked up by Render().
 */

#ifndef BEETS_BEADS_PROCESSOR_H_
#define BEETS_BEADS_PROCESSOR_H_

#include <stdint.h>
#include <string.h>
#include <new>  // placement new (BEETS_STATE_IN_SDRAM)

#include "unit_delfx.h"

#include "beets_config.h"
#include "profiler.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/hysteresis_filter.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/gate_flags.h"
#include "beads/dsp/processor.h"

// Equivalent of beads::PotChannel (cv_reader.h) for one control: the raw
// value goes through the pot law, a one-pole low-pass run once per block, and
// a hysteresis filter. Laws are applied by the caller before Process().
class BeetsPot {
 public:
  void Init(float hysteresis, float initial) {
    lp_ = initial;
    hysteresis_.Init(hysteresis);
    hysteresis_.Process(initial, 0.0f);  // start settled at the initial value
  }
  inline void Process(float v) {
    ONE_POLE(lp_, v, 0.05f);  // cv_reader.cc: pot_channel_[i].Init(law, 0.05f, ...)
    hysteresis_.Process(lp_);
  }
  inline float value() const { return hysteresis_.value(); }

 private:
  float lp_;
  stmlib::HysteresisFilter hysteresis_;
};

class BeetsProcessor {
 public:
  // Same layout as beads.cc: delay_buffer_1 (512 KB) feeds banks 0 and 1,
  // delay_buffer_2 (256 KB) is bank 2.
  enum {
    kBankSize = 262144,
    kBuffer1Size = 512 * 1024,
    kBuffer2Size = 256 * 1024,
    kTotalDelayBytes = kBuffer1Size + kBuffer2Size,
  };

  // ---- life cycle ----------------------------------------------------------

  int8_t Init(const unit_runtime_desc_t* desc) {
    hooks_ = desc->hooks;
    buffer_1_ = buffer_2_ = NULL;
    pitch_tables_ = NULL;
#ifdef BEETS_STATE_IN_SDRAM
    processor_ptr_ = NULL;
#endif
    buffer_1_ = desc->hooks.sdram_alloc(kBuffer1Size);
    if (!buffer_1_) return k_unit_err_memory;
    buffer_2_ = desc->hooks.sdram_alloc(kBuffer2Size);
    if (!buffer_2_) {
      if (hooks_.sdram_free) hooks_.sdram_free(buffer_1_);
      buffer_1_ = NULL;
      return k_unit_err_memory;
    }
    // stmlib pitch tables (see vendor/stmlib/dsp/units.cc), rebuilt here
    // instead of costing 2 KB of the unit image.
    pitch_tables_ = reinterpret_cast<float*>(desc->hooks.sdram_alloc(
        stmlib::kPitchRatioTablesSize * sizeof(float)));
    if (!pitch_tables_) {
      Teardown();
      return k_unit_err_memory;
    }
    stmlib::InitPitchRatioTables(pitch_tables_);

#ifdef BEETS_STATE_IN_SDRAM
    uint8_t* state = desc->hooks.sdram_alloc((sizeof(beads::Processor) + 3) & ~3u);
    if (!state) {
      Teardown();
      return k_unit_err_memory;
    }
    processor_ptr_ = new (state) beads::Processor;
#endif
    uint8_t* banks[3] = {buffer_1_, buffer_1_ + kBankSize, buffer_2_};

    // GranularProcessor::Init -> DelayLine::Init clears the delay memory at
    // load time (as the SDK dummy-delfx template does in unit_init). All
    // later clears are incremental (see ClearStep).
    dsp().Init(banks);
    media_ = beads::Processor::MEDIA_DIGITAL;
    mono_ = false;
    clear_offset_ = kTotalDelayBytes;

    for (int i = 0; i < BEETS_NUM_PARAMS; ++i) raw_[i] = 0;
    reset_request_ = false;
    InitControls();

    ticks_received_ = ticks_consumed_ = 0;
    clock_pulse_remaining_ = 0;
    samples_since_tick_ = BEETS_CLOCK_TIMEOUT_SAMPLES;
    previous_seed_ = stmlib::GATE_FLAG_LOW;

    note_on_events_ = note_off_events_ = 0;
    note_on_seen_ = note_off_seen_ = 0;
    all_notes_off_ = false;
    notes_held_ = 0;
    seed_locked_ = true;   // ui.cc: seed_button_locked_ = true at boot
    seed_pressed_ = false;
    seed_was_held_ = false;
    seed_held_samples_ = 0;
    seed_long_press_fired_ = false;

    bend_semitones_ = 0.0f;

    profiler_.Init(desc->frames_per_buffer);
    profiler_.ProbeSdram(buffer_1_, kBuffer1Size);
    return k_unit_err_none;
  }

  void Teardown() {
    if (hooks_.sdram_free) {
      if (buffer_1_) hooks_.sdram_free(buffer_1_);
      if (buffer_2_) hooks_.sdram_free(buffer_2_);
      if (pitch_tables_) hooks_.sdram_free(reinterpret_cast<uint8_t*>(pitch_tables_));
#ifdef BEETS_STATE_IN_SDRAM
      if (processor_ptr_) hooks_.sdram_free(reinterpret_cast<uint8_t*>(processor_ptr_));
      processor_ptr_ = NULL;
#endif
    }
    buffer_1_ = buffer_2_ = NULL;
    pitch_tables_ = NULL;
  }

  // Non-audio context: only schedule.
  void Reset() { reset_request_ = true; }

  // ---- parameters (non-audio context: cache only) ------------------------

  void SetParameter(uint8_t index, int32_t value) {
    if (index >= BEETS_NUM_PARAMS) return;
#ifdef BEETS_PROFILE
    if (index == BEETS_PARAM_MODE) {
      profiler_.Select(value);
      return;
    }
#endif
    raw_[index] = value;
  }

  const char* GetParameterString(uint8_t index, int32_t value) {
#ifdef BEETS_PROFILE
    if (index == BEETS_PARAM_MODE) return profiler_.Format(value);
#endif
    static const char* const kModeStrings[8] = {
        "DRTS", "DRTM", "CLNS", "CLNM", "VNTS", "VNTM", "DIGS", "DIGM"};
    if (index == BEETS_PARAM_MODE && value >= 0 && value < 8) {
      return kModeStrings[value];
    }
    return NULL;
  }

  // ---- events (non-audio context: count only) -----------------------------

  void TempoTick(uint32_t counter) {
    (void)counter;
    ++ticks_received_;
    profiler_.OnTick();
  }
  void NoteOn(uint8_t note, uint8_t velocity) {
    (void)note;
    (void)velocity;
    ++note_on_events_;
    profiler_.OnNoteOn();
  }
  void NoteOff(uint8_t note) {
    (void)note;
    ++note_off_events_;
  }
  void AllNoteOff() { all_notes_off_ = true; }
  void PitchBend(uint16_t bend) {
    bend_semitones_ = (float(int32_t(bend) - 0x2000) / 8192.0f) *
                      BEETS_PITCH_BEND_RANGE;
    profiler_.OnPitchBend();
  }

  // ---- audio ----------------------------------------------------------------

  void Render(const float* in, float* out, uint32_t frames) {
    profiler_.BeginRender(frames);
    profiler_.ServiceRequests();

    const uint32_t received = ticks_received_;
    const bool tick = received != ticks_consumed_;
    ticks_consumed_ = received;
    profiler_.OnDspTickInterval(tick, frames);
    if (tick) {
      clock_pulse_remaining_ = BEETS_CLOCK_PULSE_SAMPLES;
      samples_since_tick_ = 0;
    }

    if (reset_request_) {
      reset_request_ = false;
      dsp().Configure(media_, mono_);  // neutral DSP state, buffers below
      clear_offset_ = 0;
    }
    ApplyMode();
    UpdateSeedButton(frames);

    uint32_t grain_samples = 0;
    bool steady = true;
    if (ClearStep()) {
      // Delay memory is being cleared: keep the controls moving, pass the
      // dry signal through untouched.
      for (uint32_t done = 0; done < frames; done += BEETS_BLOCK_SIZE) {
        ProcessControls();
      }
      if (out != in) memcpy(out, in, frames * 2 * sizeof(float));
      steady = false;
    } else {
      for (uint32_t done = 0; done < frames; ) {
        uint32_t n = frames - done;
        if (n > BEETS_BLOCK_SIZE) n = BEETS_BLOCK_SIZE;
        RenderBlock(in + 2 * done, out + 2 * done, n);
#ifdef BEETS_PROFILE
        const int active = dsp().num_active_grains();
        profiler_.NoteGrains(active);
        grain_samples += uint32_t(active) * n;
#endif
        done += n;
      }
    }
    profiler_.EndRender(steady, grain_samples);
  }

#ifdef TEST
  // Host-test introspection only.
  float debug_size() const { return pot_size_.value(); }
  float debug_density() const { return pot_density_.value(); }
  float debug_attenuverter_rand() const { return pot_rand_.value(); }
  float debug_pitch() const { return pot_pitch_.value() + bend_semitones_; }
  bool debug_seed_locked() const { return seed_locked_; }
  bool debug_seed_pressed() const { return seed_pressed_; }
  bool debug_clock_patched() const {
    return samples_since_tick_ < BEETS_CLOCK_TIMEOUT_SAMPLES;
  }
  bool debug_clearing() const { return clear_offset_ < kTotalDelayBytes; }
  int debug_mode() const { return int(media_) * 2 + (mono_ ? 1 : 0); }
#endif

 private:
  void InitControls() {
    // Initial pot positions = header defaults, so nothing sweeps at load.
    pot_time_.Init(0.001f, 256.0f / 1023.0f);   // cv_reader.cc: TIME hysteresis
    pot_size_.Init(0.0f, 512.0f / 1023.0f);
    pot_shape_.Init(0.0f, 512.0f / 1023.0f);
    pot_density_.Init(0.0f, 512.0f / 1023.0f);
    pot_pitch_.Init(0.0f, 0.0f);
    pot_rand_.Init(0.0f, 0.0f);
    pot_prnd_.Init(0.0f, 0.0f);
    pot_feedback_.Init(0.0f, 0.0f);
    pot_dry_wet_.Init(0.0f, 0.5f);
  }

  // beads cv_reader.h, PotChannel::Process, LAW_ATTENUVERTER.
  static inline float AttenuverterLaw(float v) {  // v in [0, 1]
    v -= 0.5f;
    return (2.0f / 0.47f) * v * std::max(fabsf(v) - 0.03f, 0.0f);
  }

  // One pass of Beads' CvReader::Read for the pots: once per block.
  void ProcessControls() {
    pot_time_.Process(float(raw_[BEETS_PARAM_TIME]) / 1023.0f);
    pot_size_.Process(float(raw_[BEETS_PARAM_SIZE]) / 1023.0f);
    pot_shape_.Process(float(raw_[BEETS_PARAM_SHAPE]) / 1023.0f);
    // DENS -512..511 -> 0..1; centre (0) -> 0.5005, inside Beads' dead zone.
    pot_density_.Process(float(raw_[BEETS_PARAM_DENSITY] + 512) / 1023.0f);
    // PTCH is already in semitones (x10). Beads' LAW_DETUNE snaps the pot to
    // musical intervals; the NTS-1 parameter is set directly instead.
    pot_pitch_.Process(float(raw_[BEETS_PARAM_PITCH]) * 0.1f);
    pot_rand_.Process(AttenuverterLaw(float(raw_[BEETS_PARAM_RAND] + 100) / 200.0f));
    pot_prnd_.Process(AttenuverterLaw(float(raw_[BEETS_PARAM_PRND] + 100) / 200.0f));
    pot_feedback_.Process(float(raw_[BEETS_PARAM_FEEDBACK]) / 1023.0f);
    pot_dry_wet_.Process(float(raw_[BEETS_PARAM_MIX] + 1000) / 2000.0f);
  }

  void RenderBlock(const float* in, float* out, uint32_t n) {
    ProcessControls();

    beads::Parameters p;
    p.time.base = pot_time_.value();
    p.size.base = pot_size_.value();
    p.shape.base = pot_shape_.value();
    p.pitch.base = pot_pitch_.value() + bend_semitones_;
    // RAND shapes the grain itself, PRND where and when it plays.
    const float r = pot_rand_.value();
    p.size.attenuverter = r;
    p.shape.attenuverter = r * 0.75f;
    const float pr = pot_prnd_.value();
    p.pitch.attenuverter = pr;  // > 0 engages RandomWalkModulation
    p.time.attenuverter = pr * 0.75f;
    p.monophonic_mode = false;  // beads.cc without ENABLE_ADVANCED_FEATURES

    beads::ModulatedParameter d;
    d.base = pot_density_.value();

    // beads.cc: mix parameters are base * 1.005, constrained to [0, 1].
    beads::Processor::MixParameters m;
    m.feedback = pot_feedback_.value() * 1.005f;
    CONSTRAIN(m.feedback, 0.0f, 1.0f);
    m.dry_wet = pot_dry_wet_.value() * 1.005f;
    CONSTRAIN(m.dry_wet, 0.0f, 1.0f);

    const bool freeze_on = raw_[BEETS_PARAM_FREEZE] != 0;
    for (uint32_t i = 0; i < n; ++i) freeze_[i] = freeze_on;

    // SEED input: 4PPQN ticks as 1 ms gate pulses.
    const bool patched = samples_since_tick_ < BEETS_CLOCK_TIMEOUT_SAMPLES;
    for (uint32_t i = 0; i < n; ++i) {
      const bool high = clock_pulse_remaining_ > 0;
      if (high) --clock_pulse_remaining_;
      previous_seed_ = seed_[i] = stmlib::ExtractGateFlags(previous_seed_, high);
    }
    if (!patched) previous_seed_ = stmlib::GATE_FLAG_LOW;  // as beads.cc
    samples_since_tick_ += n;
    if (samples_since_tick_ > BEETS_CLOCK_TIMEOUT_SAMPLES)
      samples_since_tick_ = BEETS_CLOCK_TIMEOUT_SAMPLES;

    const beads::SeedButtonState seed_state = seed_locked_
        ? beads::SEED_BUTTON_STATE_LOCKED
        : (seed_pressed_ ? beads::SEED_BUTTON_STATE_ON
                         : beads::SEED_BUTTON_STATE_OFF);

    // The host may render in place; Beads reads `in` after writing `out`
    // (dry signal), so work on a copy.
    memcpy(in_block_, in, n * sizeof(beads::FloatFrame));
    beads::FloatFrame* out_frames = reinterpret_cast<beads::FloatFrame*>(out);

    dsp().Process(seed_state, patched ? seed_ : NULL, freeze_, d, p, m,
                       in_block_, out_frames, n);
#ifdef BEETS_STRESS_TEST
    dsp().WasteCPUCycles(n);
#endif
  }

  // MODE: media * 2 + mono. Only compiled-in media are accepted.
  void ApplyMode() {
#ifndef BEETS_PROFILE
    int32_t v = raw_[BEETS_PARAM_MODE];
    if (v < BEETS_MODE_MIN || v > BEETS_MODE_MAX) return;
    const beads::Processor::Media media = beads::Processor::Media(v >> 1);
    const bool mono = (v & 1) != 0;
    if (media != media_ || mono != mono_) {
      media_ = media;
      mono_ = mono;
      // Beads reconfigures and clears the delay memory on a media change;
      // the clear is spread over the next render calls.
      dsp().Configure(media_, mono_);
      clear_offset_ = 0;
    }
#endif
  }

  // ui.cc OnSwitchPressed / OnSwitchReleased / long press, driven by MIDI
  // notes: SEED counts as held while any note is held. Called once per
  // render call (button timing does not need sample accuracy).
  void UpdateSeedButton(uint32_t frames) {
    const uint32_t on = note_on_events_;
    const uint32_t off = note_off_events_;
    notes_held_ += int32_t(on - note_on_seen_) - int32_t(off - note_off_seen_);
    note_on_seen_ = on;
    note_off_seen_ = off;
    if (all_notes_off_) {
      all_notes_off_ = false;
      notes_held_ = 0;
    }
    if (notes_held_ < 0) notes_held_ = 0;
    const bool held = notes_held_ > 0;

    if (held && !seed_was_held_) {
      // OnSwitchPressed(SWITCH_SEED): unlock, button state on.
      seed_locked_ = false;
      seed_pressed_ = true;
      seed_held_samples_ = 0;
      seed_long_press_fired_ = false;
    }
    if (held) {
      seed_held_samples_ += frames;
      if (!seed_long_press_fired_ &&
          seed_held_samples_ >= uint32_t(BEETS_SEED_LONG_PRESS_MS) * 48u) {
        // Long press is reported while still held: lock, button state off.
        seed_locked_ = true;
        seed_pressed_ = false;
        seed_long_press_fired_ = true;
      }
    } else if (seed_was_held_ && !seed_long_press_fired_) {
      // OnSwitchReleased after a short press: button state off, stays unlocked.
      seed_pressed_ = false;
    }
    seed_was_held_ = held;
  }

  // Zeroes one chunk of delay memory per call. Returns true while clearing.
  // 0x00 is silence for the 16-bit (and 12-bit) Item<> layouts and 0.0f for
  // the float freeze tail. The 8-bit mu-law media would need 0xFF instead.
  bool ClearStep() {
    if (clear_offset_ >= kTotalDelayBytes) return false;
    uint32_t n = BEETS_CLEAR_CHUNK_BYTES;
    if (clear_offset_ + n > kTotalDelayBytes) n = kTotalDelayBytes - clear_offset_;
    uint8_t* p = clear_offset_ < kBuffer1Size
        ? buffer_1_ + clear_offset_
        : buffer_2_ + (clear_offset_ - kBuffer1Size);
    memset(p, 0, n);
    clear_offset_ += n;
    return true;
  }

  // -- state --
  unit_runtime_hooks_t hooks_;
  uint8_t* buffer_1_;
  uint8_t* buffer_2_;
  float* pitch_tables_;
  uint32_t clear_offset_;
  volatile bool reset_request_;

#ifdef BEETS_STATE_IN_SDRAM
  // Option: the whole beads::Processor (grains, delay controller, multitap
  // state) lives in SDRAM, freeing its RAM for more grains / both media.
  // Only sensible if the SDRAM probe (DIAG 18-21) shows the region is cached.
  inline beads::Processor& dsp() { return *processor_ptr_; }
  beads::Processor* processor_ptr_;
#else
  inline beads::Processor& dsp() { return processor_; }
  beads::Processor processor_;
#endif
  beads::Processor::Media media_;
  bool mono_;

  volatile int32_t raw_[BEETS_NUM_PARAMS];
  BeetsPot pot_time_, pot_size_, pot_shape_, pot_density_, pot_pitch_;
  BeetsPot pot_rand_, pot_prnd_, pot_feedback_, pot_dry_wet_;

  // Per-block scratch, bounded by BEETS_BLOCK_SIZE.
  beads::FloatFrame in_block_[BEETS_BLOCK_SIZE];
  bool freeze_[BEETS_BLOCK_SIZE];
  stmlib::GateFlags seed_[BEETS_BLOCK_SIZE];

  // Clock (SEED input).
  volatile uint32_t ticks_received_;
  uint32_t ticks_consumed_;
  uint32_t clock_pulse_remaining_;
  uint32_t samples_since_tick_;
  stmlib::GateFlags previous_seed_;

  // SEED button (MIDI notes).
  volatile uint32_t note_on_events_, note_off_events_;
  uint32_t note_on_seen_, note_off_seen_;
  volatile bool all_notes_off_;
  int32_t notes_held_;
  bool seed_locked_, seed_pressed_, seed_was_held_, seed_long_press_fired_;
  uint32_t seed_held_samples_;

  volatile float bend_semitones_;

  Profiler profiler_;
};

#endif  // BEETS_BEADS_PROCESSOR_H_
