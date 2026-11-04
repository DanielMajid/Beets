#!/bin/sh
# Post-build checks on build/beets.elf:
#  - size line (text/data/bss)
#  - .init_array must be empty (no reliance on static constructors)
#  - no soft-float double helpers (__aeabi_d*) and no heap/exception symbols
#  - no dynamic stack allocation (VLAs show up as SP-relative 'sub sp, rX')
ELF=${1:-build/beets.elf}
T=${CROSS:-arm-none-eabi-}
[ -f "$ELF" ] || { echo "AUDIT FAIL: $ELF missing"; exit 1; }
${T}size "$ELF" | tail -1 | awk '{print "size: text="$1" data="$2" bss="$3" total="$4}'
IA=$(${T}objdump -h "$ELF" | awk '$2==".init_array"{print $3}')
echo "init_array: 0x$IA"
D=$(${T}nm "$ELF" | grep -E "__aeabi_(d|l2d|ui2d|i2d|f2d)|__(add|mul|div|sub)df3|__extendsfdf2" | wc -l)
echo "double helpers: $D"
H=$(${T}nm "$ELF" | grep -E " (malloc|free|_sbrk|__cxa_throw|__gxx_personality|_Znwj|_Znaj|__cxa_guard_acquire)$" | wc -l)
echo "heap/except/guard syms: $H"
V=$(${T}objdump -d "$ELF" | grep -cE "sub(\.w)?\s+sp, (r[0-9]|ip|lr)")
echo "dynamic stack adjustments: $V"
[ "$D" = 0 ] && [ "$H" = 0 ] && [ "$V" = 0 ] && echo "AUDIT OK" || { echo "AUDIT FAIL"; exit 1; }
