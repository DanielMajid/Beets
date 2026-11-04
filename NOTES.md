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
