# Beets hardware test worksheet

Run the **same test** on each profiling unit, then the listening and
headroom test on the shipping unit. Write the numbers in the tables.

Firmware: ______  Date: ______  Input source: ______

## Setup (same every time)

1. Load the unit into a delfx slot and select it. Reverb off, mod fx off.
2. **Audio:** loop one fixed source into AUDIO IN (the same file every run,
   at the same level). Don't play the keyboard unless a step says to.
3. **Reset parameters** (DEL held + encoder for the menu):
   TIME ¼ (default), SIZE centre, MIX **W100**, DENS **0** (centre),
   SHPE centre, PTCH 0, FDBK 0, RAND 0, PRND 0, FRZE off.
4. **Reading DIAG:** parameter 9. Turn to a DIAG number to show its value;
   step away and back to refresh it. **DIAG 28 = RST** clears all peaks.

## The test (profiling units: P4, P8, PM8, PM16, PM32, PMX32)

| Step | Do this | Wait | Read DIAG |
|---|---|---|---|
| A | Setup, DENS centre (grains stopped) | 10 s | 2, 4, 7, 8, 9, 18, 19, 20, 21, 22, 27; then 0 → **U0** |
| B | Arp/sequencer **off**, 120 BPM | 10 s | 10 (write it down, wait 10 s, read again) |
| C | Play 3 notes, move the pitch bend | — | 25, 26. If 25 > 0, **hold one note for 5 s** to re-lock SEED |
| D | RST. Arp/sequencer **on** at 120 BPM, internal clock | 30 s | 10, 11, 12, 13, 14, 15, 16, 17 |
| E | Same as D with **external MIDI clock** at 120 BPM (USB, then DIN) | 30 s each | 11, 12, 13, 14, 15 |
| F | Clock off. RST. **Max load:** DENS fully right, SIZE fully left, FDBK max, RAND +100, PRND +100 | 30 s | 0, 1, 5, 6, 23, 24 |
| G | RST. **Delay mode:** SIZE fully right, DENS ¼ left, FDBK half, clock on | 30 s | 0, 1, 5, 6 |
| H | RST. Sweep SIZE in and out of delay mode and turn DENS end to end, quickly, for 30 s | 30 s | 1, 6. Any clicks or dropouts? |

### Results

| DIAG | Meaning | P4 | P8 | PM8 | PM16 | PM32 | PMX32 |
|---|---|---|---|---|---|---|---|
| A: 2 | cycles per sample (budget) | | | | | | |
| A: 4 | core MHz | | | | | | |
| A: 7 / 8 / 9 | frames per buffer / min / max | | | | | | |
| A: 18 / 19 / 20 | SDRAM read: sequential / random / hot | | | | | | |
| A: 21 / 22 | random:sequential ratio / Hermite read | | | | | | |
| A: 27 | trap bits (10 = divide-by-zero trap on) | | | | | | |
| A: 0 | U0, idle cycles per sample | | | | | | |
| B: 10 | ticks with transport stopped (does it rise?) | | | | | | |
| C: 25 / 26 | note-ons / pitch bends received | | | | | | |
| D: 11 / 12 / 13 | tick ms min / max / spread µs (internal) | | | | | | |
| D: 14 / 15 | tick interval seen by DSP, samples min / max | | | | | | |
| D: 16 / 17 | tick position in period, % min / max | | | | | | |
| E: 11 / 12 / 13 | tick ms min / max / spread µs (USB) | | | | | | |
| E: 11 / 12 / 13 | tick ms min / max / spread µs (DIN) | | | | | | |
| F: 0 / 1 | cycles per sample avg / peak | | | | | | |
| F: 5 / 6 | load % avg / peak | | | | | | |
| F: 23 / 24 | cycles per grain / peak grains | | | | | | |
| G: 0 / 1 | delay mode, cycles avg / peak | | | | | | |
| G: 5 / 6 | delay mode, load % avg / peak | | | | | | |
| H: 1 / 6 | sweep peak cycles / peak load % | | | | | | |
| H | clicks or dropouts? (y/n) | | | | | | |

Steps A–C only need doing once (on P4) unless the values look different on
another unit.

## Listening and headroom test (shipping unit "Beets")

Load a demanding user oscillator and a user revfx alongside Beets. Use the
same setup as above.

| # | Do this | Expected | OK? |
|---|---|---|---|
| 1 | DENS centre | wet signal silent | |
| 2 | DENS left of centre | regular grains, faster further left | |
| 3 | DENS right of centre | random grains, faster further right | |
| 4 | SIZE fully right | crossfades (~1.5 s) into echo delay | |
| 5 | SIZE back a little | crossfades back to grains | |
| 6 | RAND / PRND to ±100 | grain length, shape, pitch and position scatter | |
| 7 | FRZE on | buffer holds; grains keep playing | |
| 8 | Arp on at 120 BPM, DENS left | grains locked to clock divisions | |
| 9 | MODE DIGM | short gap (dry only), then mono with a longer buffer | |
| 10 | Step F max-load settings, play the oscillator hard for 2 min | no dropouts, no glitches | |
| 11 | Step H sweep for 1 min | no dropouts, no glitches | |

Notes:
