/* r4_frame_rate_options.h - pure parsing of the R4 Frame Rate package
 * options (mods/preloaded/packages/r4.enhancement.frame-rate). Header-only so
 * tests/test_r4_frame_rate.c can check it without the runtime. */
#ifndef R4_FRAME_RATE_OPTIONS_H
#define R4_FRAME_RATE_OPTIONS_H

#include <stdint.h>
#include <string.h>

enum { R4_FRAME_RATE_INTERPOLATE = 0, R4_FRAME_RATE_BLEND = 1 };
enum { R4_FRAME_RATE_SMOOTH = 0, R4_FRAME_RATE_SHARP = 1 };

typedef struct R4FrameRateConfig {
    uint32_t fps;     /* 0 = follow the display refresh */
    int method;       /* R4_FRAME_RATE_INTERPOLATE / _BLEND */
    int blend;        /* R4_FRAME_RATE_SMOOTH / _SHARP (Frame blend only) */
} R4FrameRateConfig;

/* "display" -> 0, "60" / "100" / "120" / "200" / "240" / "300" -> that rate.
 * Anything else (including an empty, unread option) falls back to 0, the
 * manifest default, rather than to an arbitrary number. */
static inline uint32_t r4_frame_rate_parse_rate(const char *value) {
    static const uint32_t rates[] = {60u, 100u, 120u, 200u, 240u, 300u};
    uint32_t v = 0;
    const char *p = value;
    if (!value || !*value) return 0u;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9' || v > 100000u) return 0u;
        v = v * 10u + (uint32_t)(*p - '0');
    }
    if (value[0] == '0') return 0u;   /* no leading zeros: "060" is not a choice */
    for (size_t i = 0; i < sizeof rates / sizeof rates[0]; ++i)
        if (rates[i] == v) return v;
    return 0u;
}

static inline int r4_frame_rate_parse_method(const char *value) {
    return value && strcmp(value, "blend") == 0 ? R4_FRAME_RATE_BLEND
                                               : R4_FRAME_RATE_INTERPOLATE;
}

static inline int r4_frame_rate_parse_blend(const char *value) {
    return value && strcmp(value, "sharp") == 0 ? R4_FRAME_RATE_SHARP
                                               : R4_FRAME_RATE_SMOOTH;
}

static inline void r4_frame_rate_config_defaults(R4FrameRateConfig *cfg) {
    cfg->fps = 0u;
    cfg->method = R4_FRAME_RATE_INTERPOLATE;
    cfg->blend = R4_FRAME_RATE_SMOOTH;
}

#endif
