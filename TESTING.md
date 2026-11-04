# Beets hardware test worksheet

Run the **same test (A–H)** on each profiling unit, then the listening and
headroom test on the shipping unit. Write the numbers in the tables.

Firmware: ______  Date: ______  Input source: ______

## Units to load

All in `release/` (built by `scripts/release.sh`, not in git). Each has its
own unit ID, so several can sit in different delfx slots at once.

| File | Name on device | What it is |
|---|---|---|
| `beets.nts1mkiiunit` | Beets | shipping unit: DIGS, 4 grains, no DIAG |
| `beets-p4.nts1mkiiunit` | Beets P4 | profiling, 4 grains |
| `beets-p8.nts1mkiiunit` | Beets P8 | profiling, 8 grains |
| `beets-pm8.nts1mkiiunit` | Beets PM8 | profiling, 8 grains, DSP state in external memory |
| `beets-pm16.nts1mkiiunit` | Beets PM16 | profiling, 16 grains, state in external memory |
| `beets-pm32.nts1mkiiunit` | Beets PM32 | profiling, 32 grains, state in external memory |
| `beets-pmx32.nts1mkiiunit` | Beets PMX32 | PM32 + stress test (extra worst-case grain load) |

## Parameters (current layout)

| # | Shipping unit | Profiling units |
|---:|---|---|
| 0 | TIME (knob A) | same |
| 1 | SIZE (knob B; fully right = delay mode) | same |
| 2 | MIX (DEL + knob B) | same |
| 3 DENS, 4 SHPE, 5 PTCH, 6 FDBK, 7 RAND, 8 PRND | menu | same |
| 9 | SYNC off/on | same |
| 10 | FRZE off/on | **DIAG** (readout; no freeze) |

Notes do nothing (SEED is always in Beads' auto mode). Pitch bend = ±2
octaves on PTCH. There is no MODE parameter (DIGS only).

## Setup (same every time)

1. Load the unit into a delfx slot and select it. Mod fx and reverb off
   (except in the headroom test).
2. **Audio:** loop one fixed source into AUDIO IN — same file, same level,
   every run. Don't touch the keyboard unless a step says to.
3. **Reset parameters** (hold DEL + turn the encoder for the menu):
   TIME default (¼), SIZE centre, MIX **W100**, DENS **0** (centre),
   SHPE centre, PTCH 0, FDBK 0, RAND 0, PRND 0, **SYNC off**, FRZE off.
4. **Reading DIAG (profiling units):** parameter 10, the last in the menu.
   Turn to a DIAG number to show its value; step away and back to refresh.
   **DIAG 28 = RST** clears all peaks/min/max.

## The test (profiling units)

| Step | Do this | Wait | Read DIAG |
|---|---|---|---|
| A | Setup. DENS centre = no grains | 10 s | 2, 4, 7, 8, 9, 18, 19, 20, 21, 22, 27; then 0 → **U0** |
| B | Arp/sequencer **off**, tempo 120 BPM | 10 s | 10, wait 10 s, 10 again (does it count up with transport stopped?) |
| C | Play 3 notes, move pitch bend fully up and down | — | 25, 26 (sound should not react to the notes) |
| D | RST. **SYNC on**, DENS fully left. Arp/sequencer **on**, 120 BPM, internal clock | 30 s | 10–17 |
| E | As D, **external MIDI clock** 120 BPM: USB, then DIN | 30 s each | 11–15 |
| F | Clock off, **SYNC off**. RST. **Max load:** DENS fully right, SIZE fully left (longest grains, most overlap), FDBK max, RAND +100, PRND +100 | 30 s | 0, 1, 5, 6, 23, 24 |
| G | Back to setup values. RST. **Synced delay:** SIZE fully right, DENS ¼ left, FDBK half, **SYNC on**, clock on | 30 s | 0, 1, 5, 6 |
| H | SYNC off. RST. For 30 s, quickly sweep SIZE in and out of delay mode and DENS end to end | 30 s | 1, 6. Clicks or dropouts? |

Steps A–C only need doing on P4 unless the values change between units.

### Results

| Step: DIAG | Meaning | P4 | P8 | PM8 | PM16 | PM32 | PMX32 |
|---|---|---|---|---|---|---|---|
| A: 2 | cycles per sample (whole budget) | | | | | | |
| A: 4 | core MHz | | | | | | |
| A: 7 / 8 / 9 | frames per buffer / min / max | | | | | | |
| A: 18 / 19 / 20 | memory read: sequential / random / cached | | | | | | |
| A: 21 / 22 | random÷sequential / one 4-sample read | | | | | | |
| A: 27 | trap bits (for the record; guard already in) | | | | | | |
| A: 0 | U0, idle cycles per sample | | | | | | |
| B: 10 | ticks with transport stopped: rising? (y/n) | | | | | | |
| C: 25 / 26 | note-ons / pitch bends received | | | | | | |
| D: 11 / 12 / 13 | tick ms min / max, spread µs (internal) | | | | | | |
| D: 14 / 15 | tick interval seen by DSP, samples min / max | | | | | | |
| D: 16 / 17 | tick position in audio period, % min / max | | | | | | |
| E: 11 / 12 / 13 | tick ms min / max, spread µs (USB) | | | | | | |
| E: 11 / 12 / 13 | tick ms min / max, spread µs (DIN) | | | | | | |
| F: 0 / 1 | cycles per sample avg / peak | | | | | | |
| F: 5 / 6 | load % avg / peak | | | | | | |
| F: 23 / 24 | cycles per grain / peak active grains | | | | | | |
| G: 0 / 1 | synced delay, cycles avg / peak | | | | | | |
| G: 5 / 6 | synced delay, load % avg / peak | | | | | | |
| H: 1 / 6 | sweep peak cycles / peak load % | | | | | | |
| H | clicks or dropouts? (y/n) | | | | | | |

## Listening and headroom test (shipping unit "Beets")

Load a demanding user oscillator and a user revfx next to Beets, then use
the setup above.

| # | Do this | Expected | OK? |
|---|---|---|---|
| 1 | DENS centre | wet signal silent | |
| 2 | DENS left of centre | regular grains, faster further left | |
| 3 | DENS right of centre | random grains, faster further right | |
| 4 | SIZE fully right | ~1.5 s crossfade into echo delay | |
| 5 | SIZE back a little | crossfade back to grains | |
| 6 | RAND, then PRND, to +100 and −100 | RAND: grain length/shape scatter. PRND: pitch drift and position scatter | |
| 7 | Pitch bend fully up / down | grains up / down 2 octaves | |
| 8 | Play notes | no change to the sound | |
| 9 | FRZE on, then off | buffer holds while grains keep playing; resumes recording | |
| 10 | SYNC on, arp on 120 BPM, DENS fully left, then towards centre | one grain per 16th, then slower divisions | |
| 11 | SYNC on, DENS right of centre | grains on some ticks only (probability) | |
| 12 | SYNC on, stop the clock | no new grains; SYNC off → free-running again | |
| 13 | SYNC on, SIZE fully right, change tempo | echoes follow the tempo | |
| 14 | Step F settings, play the oscillator hard for 2 min | no dropouts or glitches | |
| 15 | Step H sweeps for 1 min | no dropouts or glitches | |

Notes:
