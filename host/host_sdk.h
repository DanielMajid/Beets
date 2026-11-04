// Host-side stand-ins for the SDK runtime pieces the unit uses.
#pragma once
#include <stdlib.h>
#include <stdint.h>
static uint8_t* host_sdram_alloc(size_t n) { return (uint8_t*)calloc(1, n); }
static void host_sdram_free(const uint8_t* p) { free((void*)p); }
static size_t host_sdram_avail() { return 3u << 20; }
