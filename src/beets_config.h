/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: beets_config.h
 *
 * Every compile-time constant of the port lives here. C and C++ compatible
 * (included by header.c).
 */

#ifndef BEETS_CONFIG_H_
#define BEETS_CONFIG_H_

/* ---- Unit identity ------------------------------------------------------ */

/* Developer ID: 'DMaj' as a little-endian FourCC. Not registered in
   logue-sdk/developer_ids.md yet; change before public release if it clashes. */
#define BEETS_DEV_ID    0x6A614D44U
#define BEETS_VERSION   0x00000100U /* 0.1.0 */

/* Measurement builds (scripts/release.sh) get their own unit ID and name so
   they can sit in other slots next to the shipping unit. */
#ifndef BEETS_VARIANT
#define BEETS_VARIANT 0
#endif
#define BEETS_UNIT_ID (0x00000001U + BEETS_VARIANT)
#if BEETS_VARIANT == 0
#define BEETS_UNIT_NAME "Beets"
#elif BEETS_VARIANT == 1
#define BEETS_UNIT_NAME "Beets P4"
#elif BEETS_VARIANT == 2
#define BEETS_UNIT_NAME "Beets P8"
#elif BEETS_VARIANT == 3
#define BEETS_UNIT_NAME "Beets PM8"
#elif BEETS_VARIANT == 4
#define BEETS_UNIT_NAME "Beets PM16"
#elif BEETS_VARIANT == 5
#define BEETS_UNIT_NAME "Beets PM32"
#elif BEETS_VARIANT == 6
#define BEETS_UNIT_NAME "Beets PMX32"
#elif BEETS_VARIANT == 7
#define BEETS_UNIT_NAME "Beets G8"
#elif BEETS_VARIANT == 8
#define BEETS_UNIT_NAME "Beets G16"
#elif BEETS_VARIANT == 9
#define BEETS_UNIT_NAME "Beets G32"
#else
#error "unknown BEETS_VARIANT"
#endif

/* ---- Granular engine ---------------------------------------------------- */

/* The single compile-time grain count. Beads uses 32 on its own MCU with
   on-chip SRAM. Override from the command line:
    ./build.sh BEETS_DEFS=-DBEETS_NUM_GRAINS=8
   The scheduler (granular_player.h, SortGrainList) touches 4 "safety"
   grains, so 4 is the hard minimum. */
#ifndef BEETS_NUM_GRAINS
#define BEETS_NUM_GRAINS 4
#endif


/* Beads runs its DSP in blocks of sample_rate / kBlockRate frames
   (beads.cc: kBlockRate = 4000 -> 12 frames at 48 kHz). Parameters,
   RandomWalkModulation steps and parameter interpolation are evaluated per
   block, so the host buffer is split into blocks of exactly this size to
   keep the control rate identical to the module. It also bounds every
   scratch buffer that replaces a Beads VLA. */
#define BEETS_BLOCK_SIZE 12

/* Upper bound on the size argument of every Beads Process()/Render() call;
   sizes the static buffers that replace the VLAs in processor.cc,
   granular_processor.h and trigger_generator.cc. Beads itself allows 24
   (kMaxCodecBlockSize) but this port never passes more than one block, and
   every frame of headroom costs 116 bytes of the 24 KB RAM budget. */
#define BEETS_MAX_BLOCK_SIZE BEETS_BLOCK_SIZE

/* Bytes of delay memory zeroed per render call after unit_reset().
   768 KB / 16 KB = 48 render calls. */
#define BEETS_CLEAR_CHUNK_BYTES (16 * 1024)

/* ---- Media and slot 9 ------------------------------------------------------
   Only the DIGITAL media (16-bit) is compiled in. DIGM (mono, 8 s buffer)
   costs ~2.9 KB of code - room for ~36 grains - so it is off by default but
   kept buildable with -DBEETS_ENABLE_MONO=1.

   Slot 9 depends on that choice:
     BEETS_ENABLE_MONO=0 (default): "SYNC" on/off.
     BEETS_ENABLE_MONO=1: "MODE", 4 strings = mono + 2 * sync:
                          DIGS, DIGM, SYNS (stereo + sync), SYNM (mono + sync).
   Profiling builds always use BEETS_ENABLE_MONO=0 (keeps them inside 24 KB;
   stereo is the heavier path) and put the DIAG readout in slot 10. */
#ifndef BEETS_ENABLE_MONO
#define BEETS_ENABLE_MONO 0
#endif
#if defined(BEETS_PROFILE) && BEETS_ENABLE_MONO
#error "profiling builds are DIGS-only (DIAG replaces FRZE, size budget)"
#endif
#define BEETS_MONO_ENABLED BEETS_ENABLE_MONO

/* Optional media, each a full GranularProcessor template instance:
   define BEETS_MEDIA_VINTAGE_DIGITAL, BEETS_MEDIA_CLEAN_TAPE or
   BEETS_MEDIA_DIRTY_TAPE to compile one in (not wired to MODE yet).
   BEETS_STRESS_TEST: calls Processor::WasteCPUCycles() from every block
   (upstream's dummy granular player) as the worst-case load. */

/* ---- SEED and clock --------------------------------------------------------
   SEED is always in Beads' default "locked" state (free-running grains); the
   NTS-1 has no SEED button. SYNC on connects the 4PPQN clock to Beads' SEED
   input, as if a clock were patched into the jack. */
/* A 4PPQN tick becomes a gate pulse this long on the SEED input (1 ms). */
#define BEETS_CLOCK_PULSE_SAMPLES 48
/* Pitch bend range, semitones each way, added to PTCH (+/- 2 octaves). */
#define BEETS_PITCH_BEND_RANGE 24.0f

/* ---- Parameter indices -------------------------------------------------- */
enum {
  BEETS_PARAM_TIME = 0,
  BEETS_PARAM_SIZE,
  BEETS_PARAM_MIX,
  BEETS_PARAM_DENSITY,
  BEETS_PARAM_SHAPE,
  BEETS_PARAM_PITCH,
  BEETS_PARAM_FEEDBACK,
  BEETS_PARAM_RAND,
  BEETS_PARAM_PRND,
  BEETS_PARAM_SYNC,   /* SYNC, or MODE (mono + 2 * sync) with BEETS_ENABLE_MONO */
  BEETS_PARAM_FREEZE, /* DIAG in BEETS_PROFILE builds */
  BEETS_NUM_PARAMS
};

/* ---- Diagnostics (BEETS_PROFILE builds) --------------------------------- */
#define BEETS_DIAG_COUNT 29 /* == DIAG_LAST in profiler.h (static_assert there) */

#endif /* BEETS_CONFIG_H_ */
