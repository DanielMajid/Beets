// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// The whole FX chain.

#ifndef BEADS_DSP_PROCESSOR_H_
#define BEADS_DSP_PROCESSOR_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/granular_processor.h"
#include "beads/dsp/modulated_parameter.h"
// Beets: fx/reverb.h removed (reverb stage deleted; the NTS-1 revfx slot follows).
#include "beads/dsp/fx/wow_and_flutter.h"

#include "beets_config.h"  // Beets: BEETS_MEDIA_* switches

// Setting this flag slows down the codec to 24kHz for the MEDIA_DIRTY_TAPE
// setting. The codec becomes very hissy at this sample rate, but this has
// a certain charm!
// 
// When this flag is not set, the codec is run at 48kHz and the wet signal is
// downsampled by a 0.5 factor before being processed. Low-pass filtering
// is also applied to the output signal within the wow and flutter code.
// Beets: ENABLE_HARDWARE_24K left undefined - the NTS-1 codec rate is fixed at 48 kHz.
// #define ENABLE_HARDWARE_24K

namespace beads {

// (512 + 256) * 1000 bytes, 48kHz 16-bit stereo = 4 seconds
const size_t kDelayBufferSizeBits = 768 * 1000 * 8;

class Processor {
 public:
  Processor() { }
  ~Processor() { }
  
  enum Media {
    MEDIA_DIRTY_TAPE,
    MEDIA_CLEAN_TAPE,
    MEDIA_VINTAGE_DIGITAL,
    MEDIA_DIGITAL
  };
  
  // Beets: reverb_amount (reverb deleted) and output_gate (no gate output) removed.
  struct MixParameters {
    float feedback;
    float dry_wet;
  };
  
  // Beets: reverb_buffer argument removed.
  void Init(uint8_t* buffers[]);
  void Configure(Media media, bool mono);
  
  inline Media media() const { return media_; }
#ifdef BEETS_PROFILE  // Beets: read-only accessor for the profiler (digital media).
  inline int num_active_grains() const {
    return processor_digital_.num_active_grains();
  }
#endif
  inline bool mono() const { return mono_; }
  
  inline float sample_rate() const {
#ifdef ENABLE_HARDWARE_24K
    float sr[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };
#else
    float sr[4] = { 48000.0f, 48000.0f, 32000.0f, 48000.0f };
#endif  // ENABLE_HARDWARE_24K
    return sr[media_];
  }
  
  inline int ratio() const {
#ifdef ENABLE_HARDWARE_24K
    int ratio[4] = { 1, 2, 1, 1 };
#else
    int ratio[4] = { 2, 2, 1, 1 };
#endif  // ENABLE_HARDWARE_24K
    return ratio[media_];
  }
  
  inline uint8_t* delay_buffer_raw_bytes(size_t offset) {
    // Independently of the quality settings, we're writing in the same
    // bytes anyway!
    // Beets: digital instance (always compiled in) instead of vintage digital.
    return processor_digital_.delay_buffer_raw_bytes(mono_, offset);
  }
  
  inline int* mutable_delay_write_ptr() {
    switch (media_) {
#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_DIRTY_TAPE:
        return processor_dirty_tape_.mutable_delay_write_ptr(mono_);
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_CLEAN_TAPE:
        return processor_clean_tape_.mutable_delay_write_ptr(mono_);
#endif
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL  // Beets: media compiled in only on demand.
      case MEDIA_VINTAGE_DIGITAL:
        return processor_vintage_digital_.mutable_delay_write_ptr(mono_);
#endif
      case MEDIA_DIGITAL:
      default:
        return processor_digital_.mutable_delay_write_ptr(mono_);
    }
  }
  
  inline float overwrite_reserve() const {
    float reserve = float(remaining());
    float fade_duration = sample_rate() * 0.15f / float(ratio());
    return reserve >= fade_duration ? 1.0f : reserve / fade_duration;
  }
  
  inline int remaining() const {
    switch (media_) {
#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_DIRTY_TAPE:
        return processor_dirty_tape_.remaining(mono_);
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_CLEAN_TAPE:
        return processor_clean_tape_.remaining(mono_);
#endif
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL  // Beets: media compiled in only on demand.
      case MEDIA_VINTAGE_DIGITAL:
        return processor_vintage_digital_.remaining(mono_);
#endif
      case MEDIA_DIGITAL:
      default:
        return processor_digital_.remaining(mono_);
    }
  }

  inline void Trim() {
    switch (media_) {
#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_DIRTY_TAPE:
        processor_dirty_tape_.Trim(mono_);
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE  // Beets: media compiled in only on demand.
      case MEDIA_CLEAN_TAPE:
        processor_clean_tape_.Trim(mono_);
#endif
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL  // Beets: media compiled in only on demand.
      case MEDIA_VINTAGE_DIGITAL:
        processor_vintage_digital_.Trim(mono_);
#endif
      case MEDIA_DIGITAL:
      default:
        processor_digital_.Trim(mono_);
    }
  }
  
#ifdef BEETS_STRESS_TEST
  // Beets: stress-test builds only. Uses vintage digital when compiled in
  // (upstream choice), else the digital instance.
  void WasteCPUCycles(size_t size) {
    // The one with the most convoluted memory access!
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL
    processor_vintage_digital_.WasteCPUCycles(size);
#else
    processor_digital_.WasteCPUCycles(size);
#endif
  }
#endif  // BEETS_STRESS_TEST

  void Process(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const MixParameters& mix_parameters,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size);
  
 private:
  GranularProcessor<
      RESOLUTION_16_BIT,
      kDelayBufferSizeBits / (16 * 2),
      1> processor_digital_;
  // Beets: only the digital (16-bit, ratio 1) instance is always compiled;
  // every other media costs a full template instance of code size.
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL
  GranularProcessor<
      RESOLUTION_12_BIT,
      kDelayBufferSizeBits / (12 * 2),
      1> processor_vintage_digital_;
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE
  GranularProcessor<
      RESOLUTION_12_BIT,
      kDelayBufferSizeBits / (12 * 2),
      2> processor_clean_tape_;
#endif
#ifdef BEETS_MEDIA_DIRTY_TAPE
#ifdef ENABLE_HARDWARE_24K
  GranularProcessor<
      RESOLUTION_8_BIT,
      kDelayBufferSizeBits / (8 * 2),
      1> processor_dirty_tape_;
#else
  GranularProcessor<
      RESOLUTION_8_BIT,
      kDelayBufferSizeBits / (8 * 2),
      2> processor_dirty_tape_;
#endif  // ENABLE_HARDWARE_24K
#endif  // BEETS_MEDIA_DIRTY_TAPE

  TriggerGenerator trigger_generator_;
  DelayController delay_controller_;
  
  Media media_;
  bool process_dry_;
  bool mono_;
  
  float delay_mode_transition_;

  float dry_;
  float wet_;
  
#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: wow and flutter is only used by dirty tape.
  WowAndFlutter wow_and_flutter_;
#endif
  // Beets: reverb_ and pre_reverb_limiter_ removed with the reverb stage.
  Limiter output_limiter_;
  
  DISALLOW_COPY_AND_ASSIGN(Processor);
};

}  // namespace beads

#endif  // BEADS_DSP_PROCESSOR_H_
