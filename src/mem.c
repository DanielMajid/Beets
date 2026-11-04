/*
 * Beets - NTS-1 digital kit mkII delfx unit.
 *
 * File: mem.c
 *
 * Small memcpy / memmove / memset. newlib-nano's versions cost 724 bytes of
 * the 24 KB unit; these cost a fraction of that. Callers: 96-byte block
 * copies, std::copy / std::rotate inside Beads, and the 16 KB incremental
 * delay-memory clear (word stores when aligned, which SDRAM bandwidth caps
 * anyway).
 *
 * The attribute stops GCC from recognising the loops and emitting calls to
 * the very functions being defined.
 */

#include <stddef.h>
#include <stdint.h>

#define BEETS_NO_LIBCALL \
  __attribute__((optimize("no-tree-loop-distribute-patterns")))

BEETS_NO_LIBCALL void* memset(void* dst, int c, size_t n) {
  uint8_t* d = (uint8_t*)dst;
  if ((((uintptr_t)d) & 3) == 0) {
    const uint32_t w = 0x01010101u * (uint8_t)c;
    uint32_t* d32 = (uint32_t*)d;
    for (; n >= 4; n -= 4) *d32++ = w;
    d = (uint8_t*)d32;
  }
  while (n--) *d++ = (uint8_t)c;
  return dst;
}

BEETS_NO_LIBCALL void* memcpy(void* __restrict dst, const void* __restrict src,
                              size_t n) {
  uint8_t* d = (uint8_t*)dst;
  const uint8_t* s = (const uint8_t*)src;
  if (((((uintptr_t)d) | ((uintptr_t)s)) & 3) == 0) {
    uint32_t* d32 = (uint32_t*)d;
    const uint32_t* s32 = (const uint32_t*)s;
    for (; n >= 4; n -= 4) *d32++ = *s32++;
    d = (uint8_t*)d32;
    s = (const uint8_t*)s32;
  }
  while (n--) *d++ = *s++;
  return dst;
}

BEETS_NO_LIBCALL void* memmove(void* dst, const void* src, size_t n) {
  uint8_t* d = (uint8_t*)dst;
  const uint8_t* s = (const uint8_t*)src;
  if (d <= s || d >= s + n) return memcpy(dst, src, n);
  d += n;
  s += n;
  while (n--) *--d = *--s;
  return dst;
}
