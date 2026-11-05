# Stress testing Beets

The goal: push the full Beets unit as hard as possible while the NTS-1 is
busy with other effects, and find out **whether it ever clicks, drops out,
stutters or freezes**. You'll do it by ear; no numbers to read.

You'll run the same set of tests on up to four builds, which differ only in
how many grains they can play at once. More grains = thicker texture, but
more work for the processor. The result tells us how many grains the final
unit can afford.

| File | Name on the NTS-1 | Grains |
|---|---|---|
| `beets.nts1mkiiunit` | Beets | 4 (current) |
| `beets-g8.nts1mkiiunit` | Beets G8 | 8 |
| `beets-g16.nts1mkiiunit` | Beets G16 | 16 |
| `beets-g32.nts1mkiiunit` | Beets G32 | 32 (the original module's count) |

They have different names, so all four can be loaded at once.

**Suggested order:** run everything on **Beets** first. Then go straight
to **G32**. If G32 passes everything, stop: that's the answer. If it fails,
try G16, then G8.

---

## What counts as a failure

Listen carefully, ideally on headphones. Any of these is a **fail**:

- **Click or pop** that isn't in the input or the grains themselves
- **Dropout**: the sound cuts out for a moment
- **Crackle or buzz**: rough, broken-up audio
- **Stutter**: the whole output hiccups or repeats
- **Sluggish controls**: knobs or display respond late
- **Freeze or reboot**: the NTS-1 locks up or restarts

Normal and **not** a failure: the grains' own clicks at very short sizes,
silence with DENS at centre, a ~1.5 s crossfade when SIZE enters or leaves
echo mode, and a short dry-only moment after switching units.

If something fails, note **which test, which unit, and what you heard**,
then carry on with the next test.

---

## Setup: make the NTS-1 busy

Do this once. Keep it for every test.

1. **Load the heaviest user oscillator you have** (a wavetable, FM or
   multi-voice one) and select it.
2. **Load and select a user reverb** in the reverb slot, mix about halfway.
3. **Mod effect on** (any chorus/flanger), so all three effect slots are
   running.
4. **Arpeggiator on**, playing a fast pattern at **140 BPM**, with a
   held chord so the oscillator is always sounding.
5. **Also loop an audio file into AUDIO IN**, so Beets always has plenty
   of material.
6. **Select the Beets unit under test** in the delay slot.

**Starting position for Beets** (hold DEL + encoder for the menu):

| Parameter | Set to |
|---|---|
| TIME | default |
| SIZE | centre |
| MIX | fully wet (W100) |
| DENS | centre |
| SHPE | centre |
| PTCH | 0 |
| FDBK | 0 |
| RAND, PRND | 0 |
| SYNC | off |
| FRZE | off |

Go back to this starting position before each test.

---

## The tests

Each test says what to set, how long to run it, and what to do while it
runs. Tick **pass** or **fail** in the results table at the end.

**Test 1 — Maximum grain density (3 minutes)**

- DENS **fully right** (fastest, random)
- SIZE **fully left** (long grains that overlap the most)
- FDBK **fully right**
- RAND **+100**, PRND **+100**

Leave it running for 3 minutes. Listen for any failure.

**Test 2 — Fast regular grains (2 minutes)**

As test 1, but DENS **fully left** (fastest regular rate). 2 minutes.

**Test 3 — Extreme pitch (2 minutes)**

As test 1, plus PTCH **+24**. After a minute, hold the **pitch bend fully
up** for 30 seconds, then **fully down** for 30 seconds. Pitch bend adds
up to two more octaves, so this reads the recording very fast.

**Test 4 — Echo mode at full feedback (2 minutes)**

- SIZE **fully right** (wait for the crossfade into echo mode)
- FDBK **fully right**
- DENS **three-quarters right**
- RAND and PRND **+100**

2 minutes. Loud, self-oscillating echoes are expected; failures are
clicks, dropouts or stutter *on top of* that.

**Test 5 — Synced echo with tempo changes (2 minutes)**

As test 4, plus SYNC **on**. While it runs, change the tempo every few
seconds: down to 60 BPM, up to 240 BPM, and back. The echoes should
follow without breaking up.

**Test 6 — Synced grains (2 minutes)**

Starting position, then SYNC **on**, DENS **fully left**, SIZE **fully
left**, FDBK **fully right**. Every 20 seconds, move DENS one step towards
centre (bigger clock divisions), then jump to the **right** side
(random skipping).

**Test 7 — Mode switching (1 minute)**

From test 1's settings, sweep **SIZE** back and forth across fully-right
(in and out of echo mode) as fast as you can for 1 minute, while turning
**DENS** end to end with the other hand.

**Test 8 — Freeze hammering (1 minute)**

Test 1's settings. Toggle **FRZE** on and off rapidly, about twice a
second, for 1 minute.

**Test 9 — Clock start/stop (1 minute)**

Test 6's settings. Stop and start the arpeggiator every few seconds. With
the clock stopped, no new grains are expected; when it restarts, grains
should come back cleanly.

**Test 10 — Unit switching (1 minute)**

While test 1 is running, switch the delay slot to another effect and back
to Beets, five times. Each time it comes back, expect a short dry-only
moment, then grains. Also try saving and recalling a program that uses
Beets.

**Test 11 — Long soak (20 minutes)**

Test 1's settings. Leave it running for 20 minutes, checking in every few
minutes. Anything that only goes wrong after a while (slow drift, a
freeze, growing crackle) shows up here.

---

## Results

| Test | Beets (4) | G8 | G16 | G32 |
|---|---|---|---|---|
| 1. Maximum grain density | | | | |
| 2. Fast regular grains | | | | |
| 3. Extreme pitch | | | | |
| 4. Echo, full feedback | | | | |
| 5. Synced echo, tempo changes | | | | |
| 6. Synced grains | | | | |
| 7. Mode switching | | | | |
| 8. Freeze hammering | | | | |
| 9. Clock start/stop | | | | |
| 10. Unit switching | | | | |
| 11. Long soak | | | | |

Write **P** for pass or **F** for fail. For every F, add a note below:
which test, which unit, and what you heard.

Oscillator used: ____________  Reverb used: ____________
Mod effect used: ____________  Firmware: ________  Date: ________

Notes:
