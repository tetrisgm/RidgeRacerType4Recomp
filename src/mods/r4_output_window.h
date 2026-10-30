#ifndef R4_OUTPUT_WINDOW_H
#define R4_OUTPUT_WINDOW_H
#include <stdint.h>

/* Snapshot windows share one rational clock, including after missed windows.
 * Never accumulate rounded durations or advance a fractional carry on a call
 * that reserves no window: a one-nanosecond slip can turn phase zero into an
 * expensive near-one worker draw in the preceding half-open window. */
typedef struct R4OutputWindow {
    uint64_t origin, index, numerator;
    uint32_t denominator;
} R4OutputWindow;

static uint64_t r4_output_boundary(const R4OutputWindow *w, uint64_t index) {
    const uint64_t q = w->numerator / w->denominator;
    const uint64_t r = w->numerator % w->denominator;
    return w->origin + index * q + (index / w->denominator) * r +
        (index % w->denominator) * r / w->denominator;
}
static void r4_output_start(R4OutputWindow *w, uint64_t now, uint32_t vblank_cycles) {
    w->numerator = (uint64_t)vblank_cycles * 2000000000ull;
    w->denominator = 33868800u;
    w->index = 0;
    w->origin = now + w->numerator / w->denominator;
}
/* Skip only windows whose start is already past, keeping the original grid.
 * The quotient estimates the index without overflowing elapsed*numerator.
 * Correct against exact rational boundaries before using it. */
static void r4_output_skip_elapsed(R4OutputWindow *w, uint64_t now) {
    if (r4_output_boundary(w, w->index) >= now) return;
    uint64_t index = (now - w->origin) / (w->numerator / w->denominator);
    while (index && r4_output_boundary(w, index - 1) >= now) --index;
    while (r4_output_boundary(w, index) < now) ++index;
    w->index = index;
}
static int r4_output_lead_excess(const R4OutputWindow *w, uint64_t now, int max_windows) {
    const uint64_t begin = r4_output_boundary(w, w->index);
    const uint64_t duration = r4_output_boundary(w, w->index + 1) - begin;
    return max_windows > 0 && begin > now &&
        begin - now > duration * (uint64_t)max_windows;
}
#endif
