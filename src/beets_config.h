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
#define BEETS_UNIT_ID   0x00000001U
#define BEETS_VERSION   0x00000100U /* 0.1.0 */
#define BEETS_UNIT_NAME "Beets"

/* ---- Granular engine ---------------------------------------------------- */

/* The single compile-time grain count. Beads uses 32 on its own MCU with
   on-chip SRAM. Override from the command line:
     ./build.sh UDEFS=-DBEETS_NUM_GRAINS=8
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

/* Bytes of delay memory zeroed per render call after unit_reset().
   768 KB / 16 KB = 48 render calls. */
#define BEETS_CLEAR_CHUNK_BYTES (16 * 1024)

/* ---- Media ---------------------------------------------------------------
   MODE value = media * 2 + mono, media numbering as beads::Processor::Media:
   0 DIRTY_TAPE, 1 CLEAN_TAPE, 2 VINTAGE_DIGITAL, 3 DIGITAL.
   Only DIGITAL is compiled in; the range is trimmed so the values keep
   their meaning if more media are added later. */
#define BEETS_MODE_MIN  6
#define BEETS_MODE_MAX  7
#define BEETS_MODE_INIT 6

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
#define BEETS_DIAG_COUNT 28 /* == DIAG_LAST in profiler.h (static_assert there) */

#endif /* BEETS_CONFIG_H_ */
