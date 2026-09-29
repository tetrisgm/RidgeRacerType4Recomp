/* R4 Frame Rate package option parsing (src/mods/r4_frame_rate_options.h).
 * Build/run: ctest -R r4_frame_rate */
#include <stdio.h>

#include "r4_frame_rate_options.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } } while (0)

int main(void) {
    static const struct { const char *in; uint32_t out; } rates[] = {
        {"display", 0}, {"60", 60}, {"100", 100}, {"120", 120}, {"200", 200},
        {"240", 240}, {"300", 300}, {"", 0}, {"144", 0}, {"060", 0},
        {"-60", 0}, {"60fps", 0}, {"999999999999", 0}, {"uncapped", 0},
    };
    for (size_t i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        char msg[96];
        snprintf(msg, sizeof msg, "rate \"%s\" -> %u", rates[i].in,
                 (unsigned)rates[i].out);
        CHECK(r4_frame_rate_parse_rate(rates[i].in) == rates[i].out, msg);
    }
    CHECK(r4_frame_rate_parse_rate(NULL) == 0, "missing rate -> display");
    CHECK(r4_frame_rate_parse_method("interpolate") == R4_FRAME_RATE_INTERPOLATE,
          "interpolate");
    CHECK(r4_frame_rate_parse_method("blend") == R4_FRAME_RATE_BLEND, "blend");
    CHECK(r4_frame_rate_parse_method("") == R4_FRAME_RATE_INTERPOLATE,
          "unread method -> manifest default (interpolate)");
    CHECK(r4_frame_rate_parse_method("BLEND") == R4_FRAME_RATE_INTERPOLATE,
          "values are case-sensitive manifest ids");
    CHECK(r4_frame_rate_parse_blend("sharp") == R4_FRAME_RATE_SHARP, "sharp");
    CHECK(r4_frame_rate_parse_blend("smooth") == R4_FRAME_RATE_SMOOTH, "smooth");
    CHECK(r4_frame_rate_parse_blend(NULL) == R4_FRAME_RATE_SMOOTH, "default smooth");
    {
        R4FrameRateConfig c;
        r4_frame_rate_config_defaults(&c);
        CHECK(c.fps == 0 && c.method == R4_FRAME_RATE_INTERPOLATE &&
              c.blend == R4_FRAME_RATE_SMOOTH,
              "defaults match the manifest defaults");
    }
    printf(failures ? "FAILED (%d)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
