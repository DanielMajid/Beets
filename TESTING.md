# Testing Beets on the NTS-1 mkII

This guide walks you through testing Beets on the hardware, one step at a
time. You don't need to understand the numbers you write down: they go
back into the code to choose how many grains the final unit can afford
without overloading the NTS-1.

There are two parts:

- **Part A — Measurement** (about 45 minutes). You load special
  "profiling" versions of Beets that show internal numbers on the display,
  run the same short routine on each, and write the numbers down.
- **Part B — Listening** (about 15 minutes). You load the normal unit and
  check that each feature sounds and behaves right, with other effects
  running.

---

## Before you start

**You need:**

- The NTS-1 mkII, with headphones or speakers.
- An audio source plugged into the NTS-1's **AUDIO IN** — a phone or
  computer playing one short loop on repeat. Use **the same loop at the same
  volume** for every test, so the numbers can be compared.
- Optional, for one step: a MIDI clock source (DAW or sequencer) on USB
  and/or DIN.
- A copy of this page to write on.

**The unit files** are in `release/` (they aren't in git; run
`scripts/release.sh` to rebuild them). Load all seven onto the NTS-1 with
KORG Kontrol Editor or loguecli. Each has its own name, so they can sit in
different delfx slots at the same time.

| File | Name on the NTS-1 | Used in |
|---|---|---|
| `beets-p4.nts1mkiiunit` | Beets P4 | Part A |
| `beets-p8.nts1mkiiunit` | Beets P8 | Part A |
| `beets-pm8.nts1mkiiunit` | Beets PM8 | Part A |
| `beets-pm16.nts1mkiiunit` | Beets PM16 | Part A |
| `beets-pm32.nts1mkiiunit` | Beets PM32 | Part A |
| `beets-pmx32.nts1mkiiunit` | Beets PMX32 | Part A |
| `beets.nts1mkiiunit` | Beets | Part B |

The P / PM units differ only in how many grains they run and where they
keep their working data. You run the same routine on each.

**How to reach the parameters.** Knob A is TIME, knob B is SIZE, and
DEL + knob B is MIX. Everything else is in the delay edit menu: hold
**DEL** and turn the **encoder** to choose a parameter, then set its value
with **knob B** (the logue SDK's description of delfx menus).

| Parameter | What it does |
|---|---|
| TIME | where in the recording grains start |
| SIZE | grain length; **fully right = delay (echo) mode** |
| MIX | dry/wet |
| DENS | grain rate. **Centre = no grains.** Left = regular, right = random |
| SHPE | grain envelope |
| PTCH | grain pitch |
| FDBK | feedback |
| RAND | randomises grain length and shape |
| PRND | randomises grain pitch and position |
| SYNC | off = free-running; on = follows the tempo clock |
| FRZE (normal unit) / **DIAG** (profiling units) | freeze, or the number readout |

---

## Part A — Measurement

### The starting position

Every step below begins from this. Set it now:

| Parameter | Set to |
|---|---|
| TIME | its default (about a quarter turn) |
| SIZE | centre |
| MIX | fully wet (**W100**) |
| DENS | **centre (0)** |
| SHPE | centre |
| PTCH | 0 |
| FDBK | 0 (fully left) |
| RAND, PRND | 0 |
| SYNC | **off** |
| Mod fx, reverb | off |
| Arpeggiator / sequencer | off |

### How to read a number

The profiling units have a parameter called **DIAG** at the end of the
edit menu. Its value is not a setting; it picks **which number to show**.

1. Go to DIAG.
2. Turn knob B until the DIAG value equals the number you want (e.g. `5`).
   The display shows the measurement.
3. The display doesn't update by itself. To refresh, turn one step away
   and back.

**Before each measurement step, "clear the peaks":** set DIAG to **28**
(the display shows `RST`), then turn back to the number you want. This
wipes the old highest/lowest values so you only capture the current step.

Some readings show a small `k`, meaning thousands (`115k` = 115 000).
Write down exactly what you see.

### The routine

Do steps 1–3 **once only**, on **Beets P4**. Then do steps 4–7 on **every**
profiling unit (P4, P8, PM8, PM16, PM32, PMX32), filling in one column of
the results table per unit.

---

**Step 1 — Idle numbers (P4 only)**

Starting position. DENS at centre, so no grains are playing. Wait 10
seconds, then read and write down:

| DIAG | What it is | Value |
|---:|---|---|
| 2 | total processing time available per sample | |
| 4 | processor speed (MHz) | |
| 7 | audio buffer size | |
| 18 | memory read speed: in order | |
| 19 | memory read speed: random | |
| 20 | memory read speed: cached | |
| 21 | ratio of 19 to 18 | |
| 27 | crash-protection setting (just for the record) | |

---

**Step 2 — Does the clock run on its own? (P4 only)**

Keep the arpeggiator and sequencer **off**. Read DIAG **10** (a tick
counter). Wait 10 seconds. Read DIAG 10 again.

- First reading: ______  Second reading: ______
- Did it go up? **yes / no**

---

**Step 3 — Notes and pitch bend (P4 only)**

Play three notes on the keyboard, then move the pitch bend fully up and
fully down. The sound should **not** react to the notes; that's
intended. Read:

| DIAG | What it is | Value |
|---:|---|---|
| 25 | how many notes the unit received | |
| 26 | how many pitch-bend messages it received | |

---

**Step 4 — Idle cost (every unit)**

Back to the starting position (DENS centre). Clear the peaks (DIAG 28).
Wait 10 seconds. Read **DIAG 0** → write it in row 4 of the results table.

---

**Step 5 — Maximum load (every unit)**

This makes the unit work as hard as it can. From the starting position, set:

- **DENS fully right**
- **SIZE fully left** (long, overlapping grains)
- **FDBK fully right**
- **RAND +100**, **PRND +100**

Clear the peaks (DIAG 28). Let it play for **30 seconds**, listening for
clicks or dropouts. Then read DIAG **0, 1, 5, 6, 23, 24** → rows 5a–5f.

---

**Step 6 — Tempo-synced delay (every unit)**

Back to the starting position. Then set:

- **SIZE fully right** (wait ~2 seconds for the crossfade into echo mode)
- **DENS a quarter turn left of centre**
- **FDBK halfway**
- **SYNC on**
- Arpeggiator or sequencer **on** at **120 BPM**

Clear the peaks. Wait 30 seconds. Read DIAG **0, 1, 5, 6** → rows 6a–6d.

While you're here, the clock-timing numbers (only needed on P4): read DIAG
**11, 12, 13, 14, 15, 16, 17** → rows 6e–6k.

---

**Step 7 — Fast twiddling (every unit)**

Starting position, SYNC off. Clear the peaks. For **30 seconds**, sweep
SIZE quickly back and forth through fully-right (in and out of echo mode)
while turning DENS from end to end. Read DIAG **1** and **6** → rows
7a–7b. Did you hear any clicks or dropouts? → row 7c.

---

**Optional — External MIDI clock (P4 only)**

Repeat step 6 with the NTS-1 following an external MIDI clock at 120 BPM:
once over USB, once over DIN. Read DIAG **11, 12, 13** each time → rows
8a–8f.

---

### Results table

One column per unit. Rows marked *P4* only need filling in for P4.

| Row | DIAG | What | P4 | P8 | PM8 | PM16 | PM32 | PMX32 |
|---|---:|---|---|---|---|---|---|---|
| 4 | 0 | idle cost | | | | | | |
| 5a | 0 | max load: average cost | | | | | | |
| 5b | 1 | max load: peak cost | | | | | | |
| 5c | 5 | max load: average CPU % | | | | | | |
| 5d | 6 | max load: peak CPU % | | | | | | |
| 5e | 23 | cost per grain | | | | | | |
| 5f | 24 | most grains playing at once | | | | | | |
| 5g | — | clicks or dropouts? (y/n) | | | | | | |
| 6a | 0 | synced delay: average cost | | | | | | |
| 6b | 1 | synced delay: peak cost | | | | | | |
| 6c | 5 | synced delay: average CPU % | | | | | | |
| 6d | 6 | synced delay: peak CPU % | | | | | | |
| 6e | 11 | *P4:* shortest tick gap (ms) | | | | | | |
| 6f | 12 | *P4:* longest tick gap (ms) | | | | | | |
| 6g | 13 | *P4:* tick gap spread (µs) | | | | | | |
| 6h | 14 | *P4:* shortest tick gap (samples) | | | | | | |
| 6i | 15 | *P4:* longest tick gap (samples) | | | | | | |
| 6j | 16 | *P4:* earliest tick position (%) | | | | | | |
| 6k | 17 | *P4:* latest tick position (%) | | | | | | |
| 7a | 1 | twiddling: peak cost | | | | | | |
| 7b | 6 | twiddling: peak CPU % | | | | | | |
| 7c | — | clicks or dropouts? (y/n) | | | | | | |
| 8a–c | 11/12/13 | *P4, optional:* USB clock gaps | | | | | | |
| 8d–f | 11/12/13 | *P4, optional:* DIN clock gaps | | | | | | |

---

## Part B — Listening (normal "Beets" unit)

Load **Beets** (not a P unit). Also load a demanding **user oscillator** and
a **user reverb** and turn them on, so the NTS-1 is busy, the way a real
patch would be. Set the starting position from Part A, then go through
the list. Tick each one that behaves as described.

**Grains**

- [ ] 1. **DENS centre** → the wet sound is silent. (That's correct: centre
  means "no grains".)
- [ ] 2. **DENS left of centre** → regular, evenly spaced grains; faster the
  further left you go.
- [ ] 3. **DENS right of centre** → irregular, random grains; faster the
  further right you go.
- [ ] 4. **SIZE** small to large → short blips to long smears. Below about a
  third of the way, grains play backwards.
- [ ] 5. **RAND** to +100 and −100 → grain lengths and shapes vary.
- [ ] 6. **PRND** to +100 → pitch wanders; to −100 → grains jump around in
  pitch and position.
- [ ] 7. **Pitch bend** fully up / down → grains move up / down two
  octaves.
- [ ] 8. **Play notes** → nothing changes (correct).

**Echo mode and freeze**

- [ ] 9. **SIZE fully right** → about 1.5 seconds of crossfade, then a
  tape-style echo delay.
- [ ] 10. **SIZE back a little** → crossfades back to grains.
- [ ] 11. **FRZE on** → the recording stops updating, but grains keep playing
  what's held. **FRZE off** → recording resumes.

**Tempo sync** (arpeggiator or sequencer on, 120 BPM)

- [ ] 12. **SYNC on, DENS fully left** → one grain on every 16th note.
- [ ] 13. Move DENS **towards centre** → grains on every 2nd, 3rd, 4th…
  16th note.
- [ ] 14. **DENS right of centre** → grains on *some* 16ths, at random; more
  of them further right.
- [ ] 15. **Stop the clock** with SYNC still on → no new grains. (Correct: it
  waits for the clock.) **SYNC off** → free-running grains return.
- [ ] 16. **SYNC on + SIZE fully right**, then change the tempo → the echoes
  follow the tempo.

**Stress**

- [ ] 17. Part A step 5 settings, playing the oscillator hard for **2
  minutes** → no clicks, dropouts or freezes.
- [ ] 18. Part A step 7 twiddling for **1 minute** → no clicks, dropouts or
  freezes.

---

## When you're done

Send back the results table, the step 1–3 numbers, and anything from
Part B that didn't behave as described (what you did, what you heard).

Notes:
