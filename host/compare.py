#!/usr/bin/env python3
"""compare.py ref.raw port.raw segments.txt [first_excluded_segment]
-> per-segment bit-exactness report (optionally stopping before a segment)."""
import struct, sys, array
a = array.array('f'); a.frombytes(open(sys.argv[1], 'rb').read())
b = array.array('f'); b.frombytes(open(sys.argv[2], 'rb').read())
segs = [l.rsplit(' ', 1) for l in open(sys.argv[3]).read().splitlines() if l.strip()]
if len(sys.argv) > 4:
    stop = [n for n, _ in segs].index(sys.argv[4])
    segs = segs[:stop + 1]
ia = array.array('I', a.tobytes()); ib = array.array('I', b.tobytes())
assert len(a) == len(b), (len(a), len(b))  # same scenario on both sides
worst = 0.0
exact_all = True
for (name, start), (_, end) in zip(segs, segs[1:]):
    s, e = 2 * int(start), 2 * int(end)
    diff = [i for i in range(s, e) if ia[i] != ib[i]]
    rms = (sum(x * x for x in a[s:e]) / max(1, e - s)) ** 0.5
    if diff:
        exact_all = False
        md = max(abs(a[i] - b[i]) for i in diff)
        worst = max(worst, md)
        print("%-12s DIFFERS  first at sample %d, %d/%d values, max |d| = %.3g (rms %.3g)" % (
            name, diff[0] // 2 - s // 2, len(diff), e - s, md, rms))
    else:
        print("%-12s bit-exact (%d values, rms %.3g)" % (name, e - s, rms))
print("ALL BIT-EXACT" if exact_all else "max abs difference %.3g" % worst)
