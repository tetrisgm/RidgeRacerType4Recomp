#ifndef R4_PRIMITIVE_HEAP_H
#define R4_PRIMITIVE_HEAP_H
#include <stdint.h>

/* Maximum's expanded heaps; shared with HUD packet-range validation. */
#define R4_BIG_HEAP_BASE 0x80200000u
#define R4_BIG_HEAP_SIZE 0x00100000u

static inline int r4_heap_range_valid(int buffer, uint32_t start, uint32_t end,
                                      uint32_t stock_lo, uint32_t stock_hi,
                                      int expanded)
{
    if (end < start) return 0;
    if (start >= stock_lo && end <= stock_hi) return 1;
    if (!expanded || buffer < 0 || buffer > 1) return 0;
    uint32_t lo = R4_BIG_HEAP_BASE + (uint32_t)buffer * R4_BIG_HEAP_SIZE;
    return start >= lo && end <= lo + R4_BIG_HEAP_SIZE;
}
#endif
