#!/usr/bin/env python3
"""Encode stmlib's lut_pitch_ratio_high/low losslessly (bit-exact floats).

high[i] = 2^((i-128)/12): float bits = kHighBits[(i-128) mod 12] + octave << 23
low[j]  = 2^(j/3072):     integer prefix sums of second differences (2 bits each)
Entry 256 of both is 0.0f (implicit zero of the upstream 257-element arrays).

Usage: gen_pitch_tables.py <upstream units.cc>   (prints C++ initialisers)
"""
import re, struct, sys

src = open(sys.argv[1]).read()
def bits(name):
    body = re.search(r'%s\[\] = \{(.*?)\};' % name, src, re.S).group(1)
    vals = [float(x) for x in re.findall(r'[-+]?[0-9][0-9.e+-]*', body)]
    return [struct.unpack('<I', struct.pack('<f', v))[0] for v in vals]

hi, lo = bits('lut_pitch_ratio_high'), bits('lut_pitch_ratio_low')
assert len(hi) == len(lo) == 256
base = [hi[128 + k] for k in range(12)]
for i, b in enumerate(hi):
    o, k = divmod(i - 128, 12)
    assert b == base[k] + (o << 23), i
d1 = [lo[i + 1] - lo[i] for i in range(255)]
d2 = [d1[i + 1] - d1[i] for i in range(254)]
assert all(-1 <= d <= 2 for d in d2)
packed = [0] * 64
for i, d in enumerate(d2):
    packed[i >> 2] |= (d + 1) << ((i & 3) * 2)
print('const uint32_t kHighBits[12] = {%s};' % ', '.join('0x%08x' % b for b in base))
print('const uint32_t kLowFirst = 0x%08x;' % lo[0])
print('const int32_t kLowFirstDelta = %d;' % d1[0])
print('const uint8_t kLowSecondDelta[64] = {%s};' % ', '.join('0x%02x' % b for b in packed))
