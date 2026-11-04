/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: unit.cc
 *
 * Thin __unit_callback shims. All state lives in BeetsProcessor; this file
 * only validates the runtime, caches parameter values, and forwards calls.
 *
 * Derived from logue-sdk platform/nts-1_mkii/dummy-delfx/unit.cc,
 * Copyright (c) 2023, KORG INC., BSD 3-Clause License (see THIRD_PARTY_LICENSES).
 */

#include "unit_delfx.h"
#include "utils/int_math.h"

#include "beads_processor.h"

static BeetsProcessor s_processor;

static int32_t s_cached_values[UNIT_DELFX_MAX_PARAM_COUNT];

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->input_channels != 2 || desc->output_channels != 2)
    return k_unit_err_geometry;
  if (!desc->hooks.sdram_alloc)
    return k_unit_err_memory;

  for (int id = 0; id < UNIT_DELFX_MAX_PARAM_COUNT; ++id) {
    s_cached_values[id] = static_cast<int32_t>(unit_header.params[id].init);
  }

  const int8_t err = s_processor.Init(desc);
  if (err != k_unit_err_none)
    return err;

  for (int id = 0; id < UNIT_DELFX_MAX_PARAM_COUNT; ++id) {
    s_processor.SetParameter(id, s_cached_values[id]);
  }
  return k_unit_err_none;
}

__unit_callback void unit_teardown() { s_processor.Teardown(); }

__unit_callback void unit_reset() { s_processor.Reset(); }

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  s_processor.Render(in, out, frames);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  if (id >= UNIT_DELFX_MAX_PARAM_COUNT)
    return;
  value = clipminmaxi32(unit_header.params[id].min, value,
                        unit_header.params[id].max);
  s_cached_values[id] = value;
  s_processor.SetParameter(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  if (id >= UNIT_DELFX_MAX_PARAM_COUNT)
    return 0;
  return s_cached_values[id];
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
  if (id >= UNIT_DELFX_MAX_PARAM_COUNT)
    return nullptr;
  value = clipminmaxi32(unit_header.params[id].min, value,
                        unit_header.params[id].max);
  return s_processor.GetParameterString(id, value);
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  s_processor.TempoTick(counter);
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  s_processor.NoteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) { s_processor.NoteOff(note); }

__unit_callback void unit_all_note_off() { s_processor.AllNoteOff(); }

__unit_callback void unit_pitch_bend(uint16_t bend) {
  s_processor.PitchBend(bend);
}
