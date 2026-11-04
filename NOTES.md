# Beets — measurement log

Every number here is either **measured** (stated how) or explicitly marked
**PENDING (hardware)**. Nothing in the "measured" column is inferred.

## Toolchain

The SDK pins `gcc-arm-none-eabi-10.3-2021.10`. Its download host
(developer.arm.com) is blocked from the build container, so the builds logged
here use **Ubuntu `gcc-arm-none-eabi` 13.2.1 (13.2.rel1-2)** with identical
flags from the SDK Makefile. Code size from GCC 10.3 will differ by a few
percent; rebuild with the SDK toolchain (`docker/run_interactive.sh`, then
`build nts-1_mkii/beets`) for release numbers.

The `size` "text" column includes ELF headers, `.dynsym`/`.dynstr`, `.rel.*`,
`.unit_header` (0x330 bytes) and rodata — i.e. the loaded image minus data/bss.

## Constraints, verified against logue-sdk @ e78b16c

| Constraint | Where confirmed |
|---|---|
| target `k_unit_target_nts1_mkii_delfx` = (5<<8)\|2, API `k_unit_api_2_0_0` | `common/runtime.h`; header uses `UNIT_TARGET_PLATFORM \| k_unit_module_delfx`, `UNIT_API_VERSION` |
| STM32H725, `-mfpu=fpv4-sp-d16`, `-Os` | `dummy-delfx/Makefile` (`MCU_MODEL := STM32H725xE`, `FPU_OPTS`, `OPT`), README |
| 48 kHz, reject others with `k_unit_err_samplerate` (−4) | README "Runtime Descriptor"; `runtime.h` error enum |
| stereo in/out, else `k_unit_err_geometry` (−8) | `dummy-delfx/unit.cc`, `runtime.h` |
| `frames` may be < `frames_per_buffer` | README "Runtime Descriptor" |
| ~24 KB unit / 24 KB RAM load; 3 MB SDRAM, allocate at init | README "Supported Modules", "Other Callable API Functions" |
| 11 parameters | `unit_delfx.h`: `UNIT_DELFX_MAX_PARAM_COUNT (11)`; `runtime.h`: `UNIT_MAX_PARAM_COUNT (11)` |
| heap and stack 0 | `ld/unit.ld` (`__heap_size = 0x0`, `__stack_size = 0x0`) |
| tick callback symbol | `unit.h` / `_unit_base.c`: `unit_tempo_4ppqn_tick` (README text says `..._tick_func`) |

Every build is checked by `scripts/audit.sh`: no `__aeabi_d*`/soft-double
helpers, no heap/exception/guard symbols, no dynamic stack adjustment (VLA
or alloca), `.init_array` empty.

## Size log (text / data / bss, from `arm-none-eabi-size`)

| Phase | Build | text | data | bss | total |
|---|---|---:|---:|---:|---:|
| 0 | dummy-delfx copied and renamed | 4318 | 300 | 72 | 4690 |
| 1 | 16-bit stereo DelayLine, 768 KB SDRAM, fixed 0.5 s read | 5314 | 200 | 136 | 5650 |

### Phase 1 notes

- Allocation: two `sdram_alloc` calls (512 KB + 256 KB), each null-checked to
  `k_unit_err_memory`; bank pointers laid out as `beads.cc` does
  (buffer_1, buffer_1 + 256 KB, buffer_2).
- Host test (`host/test_phase1.cc`): impulse comes back after exactly
  23 999 samples for `Read<HERMITE>(24000, 0)` with L and R independent.
  The −1 is upstream indexing, not a port artefact.
- `unit_reset()` only raises a flag; the next render calls `DelayLine::Reset(false)`
  and then zero 16 KB per call (48 calls for 768 KB) while passing dry audio.
  Initial clear in `unit_init` happens once at load time, as in the SDK template.
- Hardware: **PENDING** — audible delay and successful allocation on the
  device have not been observed yet (no hardware in the build environment).
| 2 | phase 1 + profiler compiled out (shipping config) | 5288 | 196 | 148 | 5632 |
| 2 | phase 1 + profiler (`BEETS_DEFS=-DBEETS_PROFILE`) | 7931 | 216 | 320 | 8467 |
| 3–7 | full port, first link (DIGS+DIGM, 4 grains) | 31879 | 488 | 3876 | **36243 — over** |
| 3–7 | + `ld/exports.ver` (export only `unit_*`) | 23690 | 188 | 3876 | 27754 |
| 3–7 | + scratch buffers sized to the 12-frame block | 23690 | 188 | 2760 | 26638 |
| 3–7 | + pitch tables rebuilt into SDRAM, compact `mem*` | **21374** | **188** | **2772** | **24334** |

See "Phase 3–7" below for the per-configuration matrix.

### Phase 2 — instrumentation

`src/profiler.h`, enabled with `./build.sh BEETS_DEFS=-DBEETS_PROFILE`. In that
build parameter slot 9 is **DIAG** (instead of MODE; MODE stays at its default,
DIGS, i.e. the heavier stereo path). Turn the encoder to a DIAG value to read
the figure; re-select it to refresh. Host test: `host/test_profiler.cc`
(fake counter that starts 1 M cycles before wrap; all figures checked).

| DIAG | Figure | Unit |
|---:|---|---|
| 0 | render cycles per sample, rolling avg (k = 1/256 per call) | cycles |
| 1 | render cycles per sample, peak (after first 1000 calls, not while clearing) | cycles |
| 2 | audio-period cycles per sample, avg (start-to-start of `unit_render`) | cycles |
| 3 | audio-period cycles per sample, peak | cycles |
| 4 | core clock = DIAG2 × 48 000 | MHz |
| 5 | load = render / period, avg | % |
| 6 | load, peak | % |
| 7 | `desc->frames_per_buffer` | frames |
| 8 / 9 | `frames` argument min / max | frames |
| 10 | 4PPQN ticks received (mod 10000) | count |
| 11 / 12 | tick interval min / max, wall clock | ms |
| 13 | tick interval spread (max − min) | µs |
| 14 / 15 | tick interval as the DSP sees it (block-quantised) min / max | samples |
| 16 / 17 | where in the audio period the tick lands, min / max | % of period |
| 18 | SDRAM sequential 16-bit read | cycles/read |
| 19 | SDRAM random 16-bit read over 512 KB | cycles/read |
| 20 | random read inside 4 KB (cache-resident; same loop as 19) | cycles/read |
| 21 | ratio DIAG19 / DIAG18 | — |
| 22 | 4 adjacent 16-bit reads at a random position (one Hermite fetch) | cycles |
| 23 | cycles per active grain per sample (least-squares slope, phase 3+) | cycles |
| 24 | peak simultaneously active grains (phase 3+) | grains |
| 25 | `unit_note_on` calls received | count |
| 26 | `unit_pitch_bend` calls received | count |
| 27 | `SCB->CCR` traps: 10 × DIV_0_TRP + UNALIGN_TRP | 0 / 1 / 10 / 11 |
| 28 | select to reset all peaks / min / max ("RST") | — |

Implementation notes:
- CYCCNT is enabled (DEMCR.TRCENA, DWT LAR unlock, CTRL.CYCCNTENA) but **not
  zeroed** — the firmware may use it; all figures are wrap-safe differences.
  Registers are addressed by their ARMv7-M architectural addresses because the
  SDK includes `core_cm7.h` via `arm_math.h` with `__CMSIS_GENERIC`, which
  leaves `DWT`/`CoreDebug` undeclared.
- The SDRAM probe runs once in `unit_init` over `delay_buffer_1` (512 KB, a
  power of two so every random loop indexes by mask). The cold (19) and hot
  (20) loops are instruction-identical (checked in the disassembly), so
  19 − 20 is the pure miss cost and 19 / 18 the requested ratio.
- Callback name: the README lists `unit_tempo_4ppqn_tick_func`, but
  `common/unit.h` and `_unit_base.c` declare `unit_tempo_4ppqn_tick`; the
  header is authoritative and that is what this unit defines.

#### Phase 2 measurements — PENDING (hardware)

| Quantity | Value | How |
|---|---|---|
| core clock | PENDING | DIAG 4 |
| audio period, cycles/sample (= delfx-visible budget ceiling) | PENDING | DIAG 2 |
| `frames_per_buffer` | PENDING | DIAG 7, and 8/9 for the actual range |
| SDRAM seq / random / hot cycles per read, ratio | PENDING | DIAG 18–21 |
| 4PPQN interval min / max / spread, internal tempo | PENDING | DIAG 11–13 at 120 BPM |
| 4PPQN interval, external MIDI clock (USB and DIN) | PENDING | DIAG 11–13 |
| tick phase within the audio period | PENDING | DIAG 16/17: ≈ constant ⇒ firmware delivers ticks synchronously with audio |
| do ticks fire with transport stopped? | PENDING | DIAG 10 while idle |
| do note-on / pitch-bend reach a delfx? | PENDING | DIAG 25/26 while playing |
| integer divide-by-zero / unaligned traps enabled? | PENDING | DIAG 27 |

Why DIAG 14–17 matter: ticks are consumed at the start of the next render
call, so if the firmware delivers them asynchronously the DSP sees them
quantised to the buffer size. At 64 frames that is up to 1.33 ms, which is
already above `DelayClock`'s `acceptable_jitter_` (48 samples = 1 ms). If
DIAG 16/17 show the tick landing anywhere in the period, the fix is to
timestamp the tick with CYCCNT and place it at the matching sample offset —
not to touch `acceptable_jitter_` or add a smoother.


## Phases 3–7 — granular engine, delay mode, post chain, parameters

**Process note.** The phase gates in the brief need hardware measurements.
None were possible here, so phases 3–7 were written as one piece of fidelity
work that makes **no performance decision**: the grain count stays at
phase 3's value (4), and every choice that depends on cycles or SDRAM
behaviour is a compile-time option, listed below as PENDING. Correctness was
established on the host instead (next section).

### Fidelity: host comparison against upstream Beads

`make -C host fidelity EURORACK=... STMLIB=...` builds `beads::Processor`
twice — from the untouched eurorack/stmlib trees and from `vendor/` — and
drives both with identical input, parameters, SEED/clock/freeze streams in
12-frame blocks for 24 segments (6.3 M stereo samples, ≈131 s). 32 grains
(upstream's count). Segments: regular / randomised / dead-zone density,
± attenuverters (incl. pitch random walk), reverse grains, freeze/thaw,
divider and probability clocking, SEED on/off, gated clock, free-running /
pitched / synced / frozen / odd-tap delay mode, SIZE hysteresis up and down,
mono, mono delay, back to stereo.

| Comparison (input −6 dB) | Result |
|---|---|
| A: upstream with reverb replaced by an exact bypass vs port using upstream's `lut_sine` | **bit-exact, all 24 segments** |
| B: same reference vs shipping port (`fx_sinf`) | bit-exact except the 2 free-running odd-tap delay segments: max \|Δ\| 2.7e-5 (−91 dBFS) |
| C: full upstream (reverb amount 0) vs shipping port | max \|Δ\| 0.05, all accounted for below |

Every difference against full upstream has an identified cause:

| Where | max \|Δ\| | Cause |
|---|---:|---|
| first 12 samples after `Init` and after each `Configure` | 0.0036–0.05 | upstream `Reverb::Reset` sets `dry_ = 0`, so the dry path fades in over one block. Gone with the reverb. |
| everywhere else | ≤1.2e-7 | upstream reverb's dry gain settles 1 ULP short of 1.0 (`ParameterInterpolator` accumulation). |
| free-running delay, density > 0.5 | ≤3e-5 | `fx_sinf` (128-point firmware table) replaces `lut_sine` for the odd-tap LFO. Authorised swap. |
| only when the signal exceeds 0.9 (seen at full input level) | ~3e-3 | `pre_reverb_limiter_` removed as mandated; `output_limiter_` (same 0.9 threshold) still applies. |

Other host tests (`make -C host`): `test_units` (pitch tables, 0 of 514
mismatches), `test_profiler`, `test_unit` (the real `unit.cc` callbacks):
SDK error codes; SIZE 1023 > 0.99 and 1002 < 0.98; DENS 0 inside the dead
zone and wet-silent; grains on both sides of centre; attenuverter dead zone
and full scale; pitch + bend; SEED press/release/4 s re-lock; ticks →
patched, 4 s silence → unpatched; 1, 5, 12, 37 and 64-frame and in-place
renders; MODE change and `unit_reset` clear in exactly 48 calls with dry
pass-through.

### Upstream hazard found by the comparison: integer divide by zero

`DelayClock::next_interval_` only advances while the synchronised delay path
runs. If a clock edge lands on the first sample after entering synced delay
mode, `DelayClock` records a 1-sample interval, `slice_duration` becomes
`int(1 × 1/16) = 0`, and `DelayController` evaluates `max_delay_ / 0`
(`delay_controller.cc:277`). On Beads this is harmless because Cortex-M7
`SDIV` returns 0 unless `SCB->CCR.DIV_0_TRP` is set; the next tick corrects
the interval. On the NTS-1 it **crashes the instrument if the firmware sets
DIV_0_TRP**. The probability is about 16/6000 per entry into synced delay
mode at 120 BPM (any first interval below 16 samples).

`delay_controller.cc` is on the read-only list, so it is unpatched.
**PENDING (hardware): DIAG 27** shows `10 × DIV_0_TRP + UNALIGN_TRP`. If the
first digit is 1, the minimal fix is a one-line guard at line 277
(`max_delay_ / std::max(slice_duration, 1)`), which leaves every non-zero
case bit-identical. (UNALIGN_TRP matters only if the packed 12-bit media are
ever compiled in.)

### Size engineering

1. **Dynamic tables (−8.5 KB).** With `-shared`, all ~110 C++ template
   symbols were exported: `.dynstr` 4.8 KB, `.dynsym` 1.7 KB, `.plt` 1.1 KB,
   `.hash` 0.8 KB, `.rel.plt` 0.5 KB. `ld/exports.ver` exports exactly
   `unit_*` (incl. `unit_header`); the only import, `wt_sine_lut_f`, is
   unchanged.
2. **Scratch buffers (−1.1 KB bss).** The 12 VLA replacements are sized to
   one 12-frame block, the largest size ever passed (`taps_buffer` was 1.6 KB
   at Beads' 24).
3. **Pitch tables (−2 KB).** `stmlib::lut_pitch_ratio_{high,low}` are
   rebuilt at init into 2 KB of SDRAM from 112 bytes of lossless encoding
   (12 mantissas; 2-bit second differences). Verified bit-exact.
4. **`mem*` (−0.6 KB).** newlib-nano's 724 bytes replaced by `src/mem.c`.
5. `--gc-sections` was tried: −88 B. Not adopted (not worth leaving the
   SDK's link setup).

| Configuration | text | data | bss | total | fits |
|---|---:|---:|---:|---:|---|
| **default**: DIGS+DIGM, 4 grains, state in RAM | 21374 | 188 | 2772 | 24334 | yes (242 B spare) |
| DIGS+DIGM, 8 grains, state in RAM | 21450 | 188 | 3092 | 24730 | **no** |
| DIGS only, 16 grains | 18586 | 188 | 3732 | 22506 | yes |
| DIGS only, 32 grains | 18586 | 188 | 5012 | 23786 | yes |
| DIGS+DIGM, 32 grains, `BEETS_STATE_IN_SDRAM` | 21402 | 188 | 1548 | 23138 | yes |

Each grain costs **80 B** of RAM; the mono path costs **2944 B** of code.
`BEETS_STATE_IN_SDRAM` placement-news the whole `beads::Processor` (grains,
delay controller, multitap) into SDRAM. It is only acceptable if SDRAM is
cached; DIAG 18–21 decide that.

Worst-case stack (`-fstack-usage`, all frames static):
`unit_render → … → GranularProcessor::ProcessInternal (208) →
MultitapDelay::Render (128) → Tap::Render (104) → DelayLine::Read (208)`
≈ **1 KB** of the caller's stack.

### Adaptation layer (`src/beads_processor.h`)

- Host buffers are split into 12-frame blocks (Beads' 4 kHz control rate:
  `beads.cc` `kBlockRate = 4000`). Parameters, `RandomWalkModulation` steps
  and interpolators therefore run at the module's rate.
- Pots: one-pole 0.05 per block and hysteresis 0.001 on TIME, as
  `cv_reader.cc`; RAND/PRND pass through Beads' attenuverter law
  `(2/0.47)·v·max(|v|−0.03, 0)`. Mix and feedback: `× 1.005`, clamped, as `beads.cc`.
- SEED button = MIDI notes, with the `ui.cc` state machine: locked at boot;
  press → unlocked + gate; release → gate off; 4000 ms hold → locked.
- SEED input = 4PPQN tick as a 1 ms pulse. The clock counts as patched while
  ticks keep arriving and unpatches after 4 s without one (Beads'
  `kDividerResetInterval`). In locked state a patched clock routes to
  `RenderDivider` (divisions 1–16, probability skipping), exactly as a
  patched SEED jack does on Beads. No extra smoothing: `DelayClock` and
  `RenderDivider` handle jitter.
- Ticks are consumed at the start of the next render call. Whether that
  quantisation matters depends on DIAG 14–17 (see phase 2).
- MODE changes and `unit_reset` call `Processor::Configure`, then zero the
  768 KB at 16 KB per render call (48 calls); dry passes through meanwhile.
- `unit_set_param_value` only caches into `raw_[]`; all DSP state is
  touched from `unit_render`.

## Phase 4 — grain count: PENDING (hardware)

The arithmetic, to be filled with measured values:

```
B   = audio-period cycles per sample          (DIAG 2)
U0  = render cycles/sample, granular, 0 grains active (DENS centre)   (DIAG 0)
g   = cycles per active grain per sample      (DIAG 23, slope fit)
T   = target whole-unit cost = 2000..3000 cycles/sample (brief)
N   = floor((T - U0) / g), also <= RAM limit for the chosen configuration
worst case = U0 + N*g + stress (DIAG 1 with "Beets PMX32")
```

Prebuilt measurement units (`scripts/release.sh`, all fit 24 KB):

| File / name | Purpose |
|---|---|
| `beets.nts1mkiiunit` / Beets | shipping default (4 grains) |
| `beets-p4` / Beets P4, `beets-p8` / Beets P8 | profiler, state in RAM: U0 and g |
| `beets-pm8` / Beets PM8 | profiler, state in SDRAM: compare with P8 → cost of `BEETS_STATE_IN_SDRAM` |
| `beets-pm16`, `beets-pm32` | profiler, state in SDRAM, 16 / 32 grains |
| `beets-pmx32` / Beets PMX32 | + `WasteCPUCycles` stress test (upstream dummy player) |

The decision then follows from the numbers:
- if P8 ≈ PM8, ship `BEETS_STATE_IN_SDRAM` with N grains and both DIGS and DIGM;
- otherwise ship DIGS only with N ≤ 32 grains in RAM, or keep DIGM at 4.

## Phase 8 — headroom validation: PENDING (hardware)

Load a heavy user oscillator and a user revfx, then play maximum density,
minimum SIZE, maximum FDBK, SIZE sweeps through delay mode, MODE switches
and fast parameter sweeps. Record DIAG 0/1/5/6 and any dropouts.

## Hardware session checklist

1. Load `Beets P4`. Read DIAG 2, 4, 7–9, 18–22, 27 at idle.
2. 120 BPM internal tempo, transport stopped: DIAG 10 — do ticks arrive?
   Then with arp/sequencer running: DIAG 11–17. Repeat with external MIDI
   clock over USB and over DIN.
3. Play notes and bend: DIAG 25/26. If they stay 0, a delfx does not receive
   them; SEED then stays locked (Beads' default), and a menu fallback is needed.
4. DENS at centre: DIAG 0 (U0). DENS fully right/left, small SIZE: DIAG 0/23/24.
5. Repeat 4 with P8, PM8, PM16, PM32, PMX32. Fill in phase 4.
6. Phase 8 with the chosen build.
