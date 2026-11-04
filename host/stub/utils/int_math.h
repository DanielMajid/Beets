#pragma once
#include <stdint.h>
static inline int32_t clipminmaxi32(int32_t m, int32_t x, int32_t M) { return x < m ? m : (x > M ? M : x); }
