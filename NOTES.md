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
| 27 | select to reset all peaks / min / max ("RST") | — |

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

Why DIAG 14–17 matter: ticks are consumed at the start of the next render
call, so if the firmware delivers them asynchronously the DSP sees them
quantised to the buffer size. At 64 frames that is up to 1.33 ms, which is
already above `DelayClock`'s `acceptable_jitter_` (48 samples = 1 ms). If
DIAG 16/17 show the tick landing anywhere in the period, the fix is to
timestamp the tick with CYCCNT and place it at the matching sample offset —
not to touch `acceptable_jitter_` or add a smoother.
