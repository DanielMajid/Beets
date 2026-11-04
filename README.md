# Beets

A granular texture processor for the Korg Nu:Tekt NTS-1 digital kit mkII, built
as a **delay effect (delfx) unit** with logue SDK 2.0.0.

Beets is a port of **Mutable Instruments Beads by Emilie Gillet**. The
granular and delay DSP is Beads' own code (MIT license), vendored with
minimal, individually marked patches. The controls had to change, because
the NTS-1 has 11 parameters and no CV inputs; the algorithm did not.
"Beets" is not a Mutable Instruments product, and neither Mutable
Instruments nor KORG endorses it or is affiliated with it.

> **Status: not yet verified on hardware.** The unit builds and fits the
> 24 KB budget (20.9 KB), and on the host it matches upstream Beads bit for
> bit (see below). It has not yet been loaded on an NTS-1 mkII. The grain count is
> still the conservative phase-3 value of 4 until CPU and SDRAM costs are
> measured on the device with the profiling builds. See [NOTES.md](NOTES.md).

## Parameters

| # | Name | Range | Control | What it does |
|---:|---|---|---|---|
| 0 | TIME | 0–1023 | knob A | Where in the 4 s buffer grains start (delay time in delay mode) |
| 1 | SIZE | 0–1023 | knob B | Grain length; below 1/3 grains play in reverse. **Fully clockwise = delay mode** (enters above 0.99, leaves below 0.98, 1.5 s crossfade) |
| 2 | MIX | D100–W100 | DEL + knob B | Dry/wet |
| 3 | DENS | −512–511 | menu | **Bipolar.** Left of centre: regular grains, faster towards the left end. Right: randomised grains, faster towards the right end. **Centre is silence** (Beads' 0.48–0.52 dead zone). With SYNC on: left = clock divisions (÷1 at the left end to ÷16 near centre), right = probability of each tick firing a grain |
| 4 | SHPE | 0–1023 | menu | Grain envelope, from square through bell to reversed ramp |
| 5 | PTCH | ±24.0 semitones | menu | Grain transposition |
| 6 | FDBK | 0–1023 | menu | Feedback |
| 7 | RAND | −100–100 | menu | Randomises the **grain itself**: SIZE attenuverter (and its delay-mode filter), SHAPE attenuverter × 0.75. Negative = sparse random jumps, positive = continuous scatter |
| 8 | PRND | −100–100 | menu | Randomises **where and when it plays**: PITCH attenuverter (positive = random-walk pitch drift), TIME attenuverter × 0.75 |
| 9 | SYNC | off / on | menu | Connects the NTS-1 tempo clock to Beads' SEED input (see below) |
| 10 | FRZE | off / on | menu | Freeze the buffer |

RAND and PRND use Beads' attenuverter response: a small dead zone around
0, then a quadratic curve.

### Seed and clock

- **SEED is always in Beads' "auto" mode** (the module's power-on state):
  grains are generated continuously according to DENS. The NTS-1 has no
  SEED button, and MIDI notes do not affect the unit.
- **SYNC on = a clock patched into SEED.** The NTS-1's 4PPQN clock
  (internal tempo or external MIDI clock, 16th notes) drives Beads' clock
  divider: DENS left of centre picks a division, right of centre a
  probability, and delay mode (SIZE fully clockwise) syncs its taps to the
  clock. As on Beads with a patched but silent clock, **no new grains start
  while SYNC is on and the clock is stopped.**
- **Pitch bend** adds ±24 semitones (±2 octaves) to PTCH.

Whether the clock also runs while the NTS-1 transport is stopped, and
whether a delfx receives pitch bend, still has to be confirmed on the
device (TESTING.md).

## Departures from Beads

- **No reverb.** Beads' reverb stage (and its pre-reverb limiter) is removed.
  Put any revfx after Beets; the NTS-1 mkII has its own reverb slot and
  memory pool. The chain ends at Beads' output limiter.
- **No CV.** All CV inputs and the `cv`/`has_cv` paths are gone. The
  attenuverters remain: they are Beads' internal randomisation, not CV.
  The gate output is gone too.
- **Four attenuverters on two controls** (RAND, PRND), as in the table above.
- **One media type, stereo only (DIGS).** Beads' mono mode (DIGM, 8 s
  buffer) needs a second copy of the grain and delay code (~2.9 KB of the
  24 KB budget, the space of ~36 grains), so it is compiled out. Build with
  `BEETS_ENABLE_MONO=1` to bring it back: parameter 9 then becomes MODE with
  DIGS / DIGM / SYNS / SYNM (mono and sync combined). Clean tape, vintage
  digital and dirty tape are separate code instances; they can be enabled at
  build time (`BEETS_MEDIA_*`) but are not wired to a parameter.
- **No SEED button.** Beads' manual seed (press to trigger, hold to gate) is
  left out; SEED stays in auto mode and SYNC replaces patching a clock.
- **Grain ceiling.** Beads plays up to 32 grains. Here the count is a build
  constant (`BEETS_NUM_GRAINS`), currently 4, to be raised from hardware
  measurements. Each grain costs 80 bytes of the 24 KB RAM budget.
- **Pitch** is set directly in 0.1-semitone steps; Beads' knob snaps to
  musical intervals. Pitch bend (±2 octaves) is added on top.
- **One upstream guard:** Beads can divide by zero in its synced delay code
  when the first clock interval is under 16 samples. The module's CPU returns
  0 for that; the port does the same explicitly, so the NTS-1 cannot trap on it.
- The sine LFOs used by delay-mode tap modulation use the firmware's
  `fx_sinf` table. Apart from the removed reverb and pre-reverb limiter
  (which only acts on peaks above 0.9), this is the only difference the
  upstream comparison measures: at most 2.7e-5 (−91 dBFS).
- No auto-gain on the input, and no seed-input delay (Beads delays SEED by
  2.5 ms to line it up with CV sampling; there is no CV here).

## Building

Requirements: logue-sdk (with the CMSIS submodule) and arm-none-eabi GCC.

**Docker (SDK toolchain):** clone or symlink this repository as
`logue-sdk/platform/nts-1_mkii/beets`, then:

```bash
docker/run_interactive.sh
```

```bash
build nts-1_mkii/beets
```

**Out of tree, with a system toolchain:**

```bash
git clone --recursive https://github.com/korginc/logue-sdk.git ../logue-sdk
```

```bash
./build.sh LOGUE_SDK=../logue-sdk GCC_BIN_PATH=/usr/bin
```

```bash
scripts/audit.sh
```

The result is `beets.nts1mkiiunit`. `scripts/audit.sh` prints the size and
checks for soft-float doubles, heap use, VLAs and static constructors.

Build options go in `BEETS_DEFS`, for example
`./build.sh BEETS_DEFS="-DBEETS_PROFILE -DBEETS_NUM_GRAINS=8"`:

| Define | Effect |
|---|---|
| `BEETS_NUM_GRAINS=n` | grain count (≥ 4) |
| `BEETS_ENABLE_MONO=1` | bring back DIGM (+2.9 KB code; slot 9 becomes MODE) |
| `BEETS_STATE_IN_SDRAM` | keep DSP state in SDRAM instead of RAM |
| `BEETS_PROFILE` | DWT profiler; parameter 10 (FRZE) becomes a DIAG readout |
| `BEETS_STRESS_TEST` | add Beads' `WasteCPUCycles` worst-case load |

`scripts/release.sh` builds the shipping unit plus the measurement variants
into `release/` (not committed).

## Tests

```bash
make -C host LOGUE_SDK=../logue-sdk
```

```bash
make -C host fidelity LOGUE_SDK=../logue-sdk EURORACK=../eurorack STMLIB=../stmlib
```

The first runs the host unit tests. The second builds upstream Beads from
unmodified eurorack and stmlib checkouts next to the port, runs both through
24 scenarios (≈131 s of audio covering every mode) and compares the output
bit for bit. Result: identical to upstream-without-reverb when upstream's
sine table is used, and within 2.7e-5 with `fx_sinf`. The mono segments
are checked with DIGM compiled in, and the DIGS-only build is checked
identical to it on all stereo segments.

Host tests check correctness only. CPU cost has to be measured on the device.

## Layout

```
src/            unit.cc (SDK shims), header.c, beads_processor.h (adaptation
                layer), beets_config.h (all constants), profiler.h, mem.c
vendor/beads/   Beads DSP, patched where marked "// Beets:"
vendor/stmlib/  stmlib subset, same convention
ld/exports.ver  linker version script (exports only the unit API)
host/           native tests and the upstream comparison
scripts/        audit, release builds, pitch-table generator
NOTES.md        measurement log and engineering decisions
```

## Credits and licenses

- DSP: **Mutable Instruments Beads** and **stmlib** by Emilie Gillet, MIT
  license.
- Build system and unit template: KORG logue SDK, BSD 3-Clause license.
- Beets code: MIT license ([LICENSE](LICENSE)).

Full texts are in [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES). The
original copyright headers are kept in every vendored file.
