/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: beads_processor.h
 *
 * PHASE 1: delay line only. 768 KB of SDRAM, 16-bit stereo DelayLine from
 * Beads, written with the input and read back at a fixed delay.
 */

#ifndef BEETS_BEADS_PROCESSOR_H_
#define BEETS_BEADS_PROCESSOR_H_

#include <stdint.h>
#include <string.h>

#include "unit_delfx.h"

#include "beets_config.h"
#include "beads/dsp/delay_line.h"

class BeetsProcessor {
 public:
  // Same layout as beads.cc: delay_buffer_1 (512 KB) feeds banks 0 and 1,
  // delay_buffer_2 (256 KB) is bank 2.
  enum {
    kBankSize = 262144,
    kNumBanks = 3,
    kBuffer1Size = 512 * 1024,
    kBuffer2Size = 256 * 1024,
    kTotalDelayBytes = kBuffer1Size + kBuffer2Size,
    // processor.h: kDelayBufferSizeBits / (16 * 2) for RESOLUTION_16_BIT.
    kStereoMaxDelay = 768 * 1000 * 8 / (16 * 2),
    kFixedDelay = 24000,  // 0.5 s, phase 1 test only.
  };

  int8_t Init(const unit_runtime_desc_t* desc) {
    hooks_ = desc->hooks;
    frames_per_buffer_ = desc->frames_per_buffer;

    buffer_1_ = desc->hooks.sdram_alloc(kBuffer1Size);
    if (!buffer_1_) return k_unit_err_memory;
    buffer_2_ = desc->hooks.sdram_alloc(kBuffer2Size);
    if (!buffer_2_) {
      desc->hooks.sdram_free(buffer_1_);
      buffer_1_ = NULL;
      return k_unit_err_memory;
    }
    banks_[0] = buffer_1_;
    banks_[1] = buffer_1_ + kBankSize;
    banks_[2] = buffer_2_;

    // DelayLine::Init clears the full 768 KB once. This happens at load
    // time, as in the SDK's dummy-delfx template (which clears 1 MB in
    // unit_init); unit_reset() clears incrementally instead.
    delay_line_.Init(banks_);
    clear_offset_ = kTotalDelayBytes;
    return k_unit_err_none;
  }

  void Teardown() {
    if (hooks_.sdram_free) {
      if (buffer_1_) hooks_.sdram_free(buffer_1_);
      if (buffer_2_) hooks_.sdram_free(buffer_2_);
    }
    buffer_1_ = buffer_2_ = NULL;
  }

  // Called from a non-audio context: only schedule the work.
  void Reset() { reset_request_ = true; }

  void SetParameter(uint8_t index, int32_t value) {
    (void)index;
    (void)value;
  }

  const char* GetParameterString(uint8_t index, int32_t value) {
    (void)index;
    (void)value;
    return NULL;
  }

  void TempoTick(uint32_t counter) { (void)counter; }
  void NoteOn(uint8_t note, uint8_t velocity) { (void)note; (void)velocity; }
  void NoteOff(uint8_t note) { (void)note; }
  void AllNoteOff() {}
  void PitchBend(uint16_t bend) { (void)bend; }

  void Render(const float* in, float* out, uint32_t frames) {
    if (reset_request_) {
      reset_request_ = false;
      delay_line_.Reset(false);  // pointers only; memory below.
      clear_offset_ = 0;
    }
    if (ClearStep()) {
      // Buffer still being cleared: pass the dry signal through.
      memcpy(out, in, frames * 2 * sizeof(float));
      return;
    }
    for (uint32_t i = 0; i < frames; ++i) {
      beads::FloatFrame s;
      s.l = in[2 * i];
      s.r = in[2 * i + 1];
      delay_line_.Write(s);
      const beads::FloatFrame d = delay_line_.template Read<
          beads::INTERPOLATION_HERMITE>(int(kFixedDelay), 0.0f);
      out[2 * i] = s.l + 0.5f * d.l;
      out[2 * i + 1] = s.r + 0.5f * d.r;
    }
  }

 private:
  // Zeroes one chunk of delay memory per call. Returns true while clearing.
  // 0x00 is silence for the 16-bit and 12-bit Item<> layouts.
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

  unit_runtime_hooks_t hooks_;
  uint16_t frames_per_buffer_;
  uint8_t* buffer_1_;
  uint8_t* buffer_2_;
  uint8_t* banks_[kNumBanks];
  uint32_t clear_offset_;
  volatile bool reset_request_;

  beads::DelayLine<beads::RESOLUTION_16_BIT, kStereoMaxDelay, 2> delay_line_;
};

#endif  // BEETS_BEADS_PROCESSOR_H_
