/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 * A port of Mutable Instruments Beads (Emilie Gillet, MIT license).
 *
 * File: header.c
 *
 * Unit manifest and the 11 parameter descriptors.
 *
 * Derived from logue-sdk platform/nts-1_mkii/dummy-delfx/header.c,
 * Copyright (c) 2023, KORG INC., BSD 3-Clause License (see THIRD_PARTY_LICENSES).
 */

#include "unit_delfx.h"
#include "beets_config.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_delfx,
    .api = UNIT_API_VERSION,
    .dev_id = BEETS_DEV_ID,
    .unit_id = BEETS_UNIT_ID,
    .version = BEETS_VERSION,
    .name = BEETS_UNIT_NAME,
    .num_params = 11,

    .params = {
        // min, max, center, init, type, frac, frac_mode, reserved, name

        // A knob
        {0, 1023, 0, 256, k_unit_param_type_none, 1, 0, 0, {"TIME"}},
        // B knob. Fully clockwise = delay mode (Beads behaviour).
        {0, 1023, 0, 512, k_unit_param_type_none, 1, 0, 0, {"SIZE"}},
        // DEL + B knob
        {-1000, 1000, 0, 0, k_unit_param_type_drywet, 1, 1, 0, {"MIX"}},

        // Edit menu (DEL held + encoder)
        // Bipolar, centre = no grains (Beads dead zone 0.48 .. 0.52).
        {-512, 511, 0, 0, k_unit_param_type_none, 1, 0, 0, {"DENS"}},
        {0, 1023, 0, 512, k_unit_param_type_none, 1, 0, 0, {"SHPE"}},
        // +/- 24.0 semitones
        {-240, 240, 0, 0, k_unit_param_type_semi, 1, 1, 0, {"PTCH"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 1, 0, 0, {"FDBK"}},
        // SIZE + SHAPE attenuverters (shape the grain itself)
        {-100, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"RAND"}},
        // PITCH + TIME attenuverters (where and when it plays)
        {-100, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"PRND"}},
#if BEETS_ENABLE_MONO
        // mono + 2 * sync: DIGS, DIGM, SYNS, SYNM
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODE"}},
#else
        // 4PPQN clock -> Beads SEED input (clock divider / probability)
        {0, 1, 0, 0, k_unit_param_type_onoff, 0, 0, 0, {"SYNC"}},
#endif
#ifdef BEETS_PROFILE
        // Diagnostic readout replaces FRZE in profiling builds only.
        {0, BEETS_DIAG_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"DIAG"}},
#else
        {0, 1, 0, 0, k_unit_param_type_onoff, 0, 0, 0, {"FRZE"}},
#endif
    },
};
