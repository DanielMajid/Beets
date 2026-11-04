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

#include "beads/dsp/processor.h"

#include <algorithm>

#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: mu-law only needed by dirty tape.
#include "beads/dsp/mu_law.h"
#endif

namespace beads {

const float kDelayModeTransitionTime = 1.5f;  // seconds
const float kDelayModeFadeTime = 0.75f;  // seconds

using namespace std;
using namespace stmlib;

// Beets: reverb_buffer argument and reverb_.Init removed; media guarded.
void Processor::Init(uint8_t* buffers[]) {
  processor_digital_.Init(48000.0f, buffers);
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL
  processor_vintage_digital_.Init(32000.0f, buffers);
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE
  processor_clean_tape_.Init(24000.0f, buffers);
#endif
#ifdef BEETS_MEDIA_DIRTY_TAPE
  processor_dirty_tape_.Init(24000.0f, buffers);
#endif
  
  dry_ = 0.0f;
  wet_ = 0.0f;
  
#ifdef BEETS_MEDIA_DIRTY_TAPE
#ifdef ENABLE_HARDWARE_24K
  wow_and_flutter_.Init(24000.0f);
  wow_and_flutter_.set_lp(0.95f);
#else
  wow_and_flutter_.Init(48000.0f);
  wow_and_flutter_.set_lp(0.6f);
#endif  // ENABLE_HARDWARE_24K
#endif  // BEETS_MEDIA_DIRTY_TAPE

  Configure(MEDIA_DIGITAL, false);
  
  // Used for the timing of the delay to granular transition.
  delay_mode_transition_ = -kDelayModeTransitionTime;
}

void Processor::Configure(Media media, bool mono) {
  media_ = media;
  mono_ = mono;
  
  int max_delay = 0;

  switch (media_) {
    case MEDIA_DIGITAL:
      processor_digital_.Reset();
      max_delay = processor_digital_.max_delay_times_ratio();
      break;
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL  // Beets: media compiled in only on demand.
    case MEDIA_VINTAGE_DIGITAL:
      processor_vintage_digital_.Reset();
      max_delay = processor_vintage_digital_.max_delay_times_ratio();
      break;
#endif
#ifdef BEETS_MEDIA_CLEAN_TAPE
    case MEDIA_CLEAN_TAPE:
      processor_clean_tape_.Reset();
      max_delay = processor_clean_tape_.max_delay_times_ratio();
      break;
#endif
#ifdef BEETS_MEDIA_DIRTY_TAPE
    case MEDIA_DIRTY_TAPE:
      processor_dirty_tape_.Reset();
      max_delay = processor_dirty_tape_.max_delay_times_ratio();
      break;
#endif
    default:  // Beets: media not compiled in.
      break;
  }
  
  float sr = sample_rate();
  // Beets: pre_reverb_limiter_.Init removed with the reverb stage.
  output_limiter_.Init(1.0f / (sr * 6.0f), 0.9f);
  trigger_generator_.Init(sr);
  delay_controller_.Init(sr, max_delay * (mono ? 2 : 1));
  // Beets: reverb_.Reset removed.
}

void Processor::Process(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const MixParameters& mix_parameters,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  const float delay_mode = delay_mode_transition_ > 0.0f;
  const float threshold = delay_mode ? 0.98f : 0.99f;
  const float direction = parameters.size.base > threshold ? 1.0f : -1.0f;
  delay_mode_transition_ += direction * float(size) / sample_rate();
  
  CONSTRAIN(delay_mode_transition_,
            -kDelayModeTransitionTime,
            +kDelayModeTransitionTime);
  
  // Beets: expo_fm_amount removed - it only split density.cv (always 0 here)
  // between the linear and exponential FM inputs; both are passed as 0.0f.
  // Beets: triggers_buffer / taps_buffer were VLAs; gate_out (gate output
  // jack) removed.
  static TriggerGenerator::Output triggers_buffer[BEETS_MAX_BLOCK_SIZE];
  static DelayController::Output taps_buffer[BEETS_MAX_BLOCK_SIZE];

  TriggerGenerator::Output* triggers = &triggers_buffer[0];
  DelayController::Output* taps = &taps_buffer[0];
  
  // Extract grain triggers.
  if (delay_mode) {
    triggers = NULL;
    delay_controller_.Render(
        seed_button_state,
        seed_input,
        freeze,
        density.base,
        0.0f,  // Beets: was (1.0f - expo_fm_amount) * density.cv
        0.0f,  // Beets: was expo_fm_amount * density.cv
        parameters.time,
        parameters.shape,
        taps,
        size);
  } else {
    taps = NULL;
    trigger_generator_.Render(
        seed_button_state,
        seed_input,
        density.base,
        0.0f,  // Beets: was (1.0f - expo_fm_amount) * density.cv
        0.0f,  // Beets: was expo_fm_amount * density.cv
        triggers,
        size);
  }
  
  static FloatFrame dry_signal[BEETS_MAX_BLOCK_SIZE];  // Beets: was a VLA.
  
  // Granular processing.
  const float feedback = mix_parameters.feedback;
  switch (media_) {
    case MEDIA_DIGITAL:
      processor_digital_.Process(
          triggers, taps, freeze, parameters, feedback, mono_, in, out, size);
      copy(&in[0], &in[size], &dry_signal[0]);
      break;
    
#ifdef BEETS_MEDIA_VINTAGE_DIGITAL  // Beets: media compiled in only on demand.
    case MEDIA_VINTAGE_DIGITAL:
      processor_vintage_digital_.Process(
          triggers, taps, freeze, parameters, feedback, mono_, in, out, size);
      for (size_t i = 0; i < size; ++i) {
        dry_signal[i].l = static_cast<int32_t>(in[i].l * 4096.0f) / 4096.0f;
        dry_signal[i].r = static_cast<int32_t>(in[i].r * 4096.0f) / 4096.0f;
      }
      break;
#endif
    
#ifdef BEETS_MEDIA_CLEAN_TAPE
    case MEDIA_CLEAN_TAPE:
      processor_clean_tape_.Process(
          triggers, taps, freeze, parameters, feedback, mono_, in, out,
          size);
      copy(&in[0], &in[size], &dry_signal[0]);
      break;
#endif
    
#ifdef BEETS_MEDIA_DIRTY_TAPE
    case MEDIA_DIRTY_TAPE:
      processor_dirty_tape_.Process(
          triggers, taps, freeze, parameters, feedback, mono_, in, out,
          size);
      for (size_t i = 0; i < size; ++i) {
        dry_signal[i].l = MuLaw2Lin(
            Lin2MuLaw(
                stmlib::Clip16(static_cast<int32_t>(in[i].l * 32768.0f))));
        dry_signal[i].r = MuLaw2Lin(
            Lin2MuLaw(
                stmlib::Clip16(static_cast<int32_t>(in[i].r * 32768.0f))));
      }
      break;
#endif
    default:  // Beets: media not compiled in.
      break;
  }
  
  // Fade-out / Fade-in the dry signal when switching between granular and
  // Delay mode.
  const float wet_fade = SmoothStep(
      std::min(1.0f, fabsf(delay_mode_transition_) / kDelayModeFadeTime));
  
  // Dry/wet balance.
  const float post_gain_dry = 0.75f;
  const float post_gain_wet = 1.5f;
  const float dry_wet = mix_parameters.dry_wet;
  const float dry = (1.0f - dry_wet) * (1.0f + dry_wet) * post_gain_dry;
  const float wet = dry_wet * (2.0f - dry_wet) * post_gain_wet * wet_fade;

  stmlib::ParameterInterpolator dry_modulation(&dry_, dry, size);
  stmlib::ParameterInterpolator wet_modulation(&wet_, wet, size);

  for (size_t i = 0; i < size; ++i) {
    out[i] = dry_modulation.Next() * dry_signal[i] + \
        wet_modulation.Next() * out[i];
  }
  
  // Wow and flutter simulation.
#ifdef BEETS_MEDIA_DIRTY_TAPE  // Beets: only exists when dirty tape is compiled in.
  if (media_ == MEDIA_DIRTY_TAPE) {
    wow_and_flutter_.Process(out, size);
  }
#endif
  
  // Beets: pre-reverb limiter and the reverb block (caveman / reverb_mix /
  // reverb_time / reverb_lp) removed. A user revfx can follow this unit.
  
  // Final clipping/limiting
  output_limiter_.Process(out, size);
  
  // Beets: output_gate block removed (no gate output on the NTS-1).
}

}  // namespace beads