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

/* ---- Media ---------------------------------------------------------------
   MODE value = media * 2 + mono, media numbering as beads::Processor::Media:
   0 DIRTY_TAPE, 1 CLEAN_TAPE, 2 VINTAGE_DIGITAL, 3 DIGITAL.
   Only DIGITAL is compiled in; the range is trimmed so the values keep
   their meaning if more media are added later. */
#ifndef BEETS_ENABLE_MONO
#ifdef BEETS_PROFILE
/* MODE is replaced by DIAG in profiling builds, so DIGM is unreachable there;
   dropping its code keeps the instrumented build inside 24 KB. Stereo is
   the heavier path, i.e. the one worth measuring. */
#define BEETS_ENABLE_MONO 0
#else
#define BEETS_ENABLE_MONO 1
#endif
#endif
#define BEETS_MONO_ENABLED BEETS_ENABLE_MONO
#define BEETS_MODE_MIN  6
#if BEETS_ENABLE_MONO
#define BEETS_MODE_MAX  7   /* DIGS, DIGM */
#else
#define BEETS_MODE_MAX  6   /* DIGS only: mono path compiled out */
#endif
#define BEETS_MODE_INIT 6

/* Optional media, each a full GranularProcessor template instance:
   define BEETS_MEDIA_VINTAGE_DIGITAL, BEETS_MEDIA_CLEAN_TAPE or
   BEETS_MEDIA_DIRTY_TAPE to compile one in (not wired to MODE yet).
   BEETS_STRESS_TEST: calls Processor::WasteCPUCycles() from every block
   (upstream's dummy granular player) as the worst-case load. */

/* ---- SEED (note-on) and clock --------------------------------------------- */
/* Beads ui.cc: holding SEED for 4000 ms re-locks it (free-running grains). */
#define BEETS_SEED_LONG_PRESS_MS 4000
/* A 4PPQN tick becomes a gate pulse this long on the SEED input (1 ms). */
#define BEETS_CLOCK_PULSE_SAMPLES 48
/* The clock counts as "patched" while ticks keep arriving; after this many
   samples without one it is "unpatched" again (4 s, Beads'
   kDividerResetInterval). */
#define BEETS_CLOCK_TIMEOUT_SAMPLES (4 * 48000)
/* Pitch bend range, semitones each way, added to PTCH. */
#define BEETS_PITCH_BEND_RANGE 2.0f

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
  BEETS_PARAM_MODE, /* DIAG in BEETS_PROFILE builds */
  BEETS_PARAM_FREEZE,
  BEETS_NUM_PARAMS
};

/* ---- Diagnostics (BEETS_PROFILE builds) --------------------------------- */
#define BEETS_DIAG_COUNT 29 /* == DIAG_LAST in profiler.h (static_assert there) */

#endif /* BEETS_CONFIG_H_ */
