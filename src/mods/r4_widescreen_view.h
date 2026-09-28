/* r4_widescreen_view.h - the "View" option of r4.enhancement.widescreen.
 *
 * Pure helper (no runtime dependencies) so tests/test_r4_widescreen.c can
 * check it without a game build. */
#ifndef R4_WIDESCREEN_VIEW_H
#define R4_WIDESCREEN_VIEW_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t numerator;    /* fixed aspect that shapes the first window */
    uint32_t denominator;
    int fit;               /* 1: follow the window afterwards, no upper limit */
} R4WidescreenView;

/* "16:9", "21:9" and "32:9" are fixed views. "Fit" and anything unknown
 * (an old or hand-edited state.toml) follow the window, starting at 16:9. */
static inline R4WidescreenView r4_widescreen_view(const char *choice)
{
    R4WidescreenView v = { 16u, 9u, 1 };
    if (!choice) return v;
    if (!strcmp(choice, "16:9")) { v.fit = 0; }
    else if (!strcmp(choice, "21:9")) { v.numerator = 21u; v.fit = 0; }
    else if (!strcmp(choice, "32:9")) { v.numerator = 32u; v.fit = 0; }
    return v;
}

#endif /* R4_WIDESCREEN_VIEW_H */
