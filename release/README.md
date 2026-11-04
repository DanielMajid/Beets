# Prebuilt units

Built by `scripts/release.sh` with Ubuntu arm-none-eabi-gcc 13.2.1 (the
SDK pins 10.3; rebuild with the SDK toolchain for release). None of these
has been loaded on hardware yet.

| File | Name on device | Contents |
|---|---|---|
| `beets.nts1mkiiunit` | Beets | shipping default: 4 grains, DIGS/DIGM |
| `beets-p4.nts1mkiiunit` | Beets P4 | profiler (param 9 = DIAG), 4 grains |
| `beets-p8.nts1mkiiunit` | Beets P8 | profiler, 8 grains |
| `beets-pm8.nts1mkiiunit` | Beets PM8 | profiler, 8 grains, DSP state in SDRAM |
| `beets-pm16.nts1mkiiunit` | Beets PM16 | profiler, 16 grains, DSP state in SDRAM |
| `beets-pm32.nts1mkiiunit` | Beets PM32 | profiler, 32 grains, DSP state in SDRAM |
| `beets-pmx32.nts1mkiiunit` | Beets PMX32 | PM32 + `WasteCPUCycles` stress test |

Each variant has its own unit ID, so they can share the delfx slots. The
DIAG index table and the measurement procedure are in `../NOTES.md`.
