# Prebuilt units (not committed)

`scripts/release.sh` builds these here; they are kept out of git. Built so
far with Ubuntu arm-none-eabi-gcc 13.2.1 (the SDK pins 10.3; rebuild with
the SDK toolchain for a release). None has been loaded on hardware yet.

| File | Name on device | Contents |
|---|---|---|
| `beets.nts1mkiiunit` | Beets | shipping default: DIGS, 4 grains |
| `beets-p4.nts1mkiiunit` | Beets P4 | profiler (param 10 = DIAG), 4 grains |
| `beets-p8.nts1mkiiunit` | Beets P8 | profiler, 8 grains |
| `beets-pm8.nts1mkiiunit` | Beets PM8 | profiler, 8 grains, DSP state in SDRAM |
| `beets-pm16.nts1mkiiunit` | Beets PM16 | profiler, 16 grains, DSP state in SDRAM |
| `beets-pm32.nts1mkiiunit` | Beets PM32 | profiler, 32 grains, DSP state in SDRAM |
| `beets-pmx32.nts1mkiiunit` | Beets PMX32 | PM32 + `WasteCPUCycles` stress test |

Each variant has its own unit ID, so they can share the delfx slots. The
DIAG table is in `../NOTES.md`, the test procedure in `../TESTING.md`.
