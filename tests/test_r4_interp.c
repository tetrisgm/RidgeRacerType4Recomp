/* R4 interpolation math (src/mods/r4_interp_math.h) and the generated field
 * table (src/mods/r4_interp_fields.h). Build/run: ctest -R r4_interp */
#include <stdio.h>
#include <string.h>

#include "r4_interp_math.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } } while (0)

/* The race handlers' own km/h sequence (0x80117790-0x801177C4). */
static int32_t handler_kmh(int16_t raw) {
    int32_t a0 = (int32_t)raw * 160;
    int64_t prod = (int64_t)a0 * (int64_t)(int32_t)0xE070381Du;
    int32_t hi = (int32_t)(prod >> 32);
    int32_t v0 = (int32_t)((uint32_t)hi + (uint32_t)a0) >> 10;
    return v0 - (a0 >> 31);
}

static void test_lerp_and_wrap(void) {
    const R4InterpField lin = {0x10, R4_FIELD_LINEAR, R4_GROUP_BODY, 0x1000};
    const R4InterpField ang = {0x54, R4_FIELD_ANGLE12, R4_GROUP_BODY, 0x200};
    const R4InterpField angh = {0x1A8, R4_FIELD_ANGLE12_H, R4_GROUP_BODY, 0x200};
    int32_t d;

    CHECK(r4_lerp(100, 50, 0) == 100, "alpha 0 is the previous tick");
    CHECK(r4_lerp(100, 50, 32768) == 125, "alpha 1/2 is the midpoint");
    CHECK(r4_lerp(100, -50, 16384) == 88 || r4_lerp(100, -50, 16384) == 87,
          "negative deltas round to nearest");
    CHECK(r4_lerp(0x7FFFFF00, 0x200, 65535) == (int32_t)0x800000FFu ||
          r4_lerp(0x7FFFFF00, 0x200, 65535) == (int32_t)0x80000100u,
          "positions wrap like the guest's 32-bit adds");
    CHECK(r4_lerp(0, 0x3FFFF, 65535) == 0x3FFFF - 4 || r4_lerp(0, 0x3FFFF, 65535) == 0x3FFFF - 3,
          "large fine-position deltas do not overflow (int64 multiply)");

    d = r4_field_delta(ang.kind, 4090, 6);
    CHECK(d == 12, "angle 4090 -> 6 is +12 through the wrap");
    CHECK(r4_lerp(4090, d, 32768) == 4096, "midpoint crosses 0x1000 forward");
    d = r4_field_delta(ang.kind, 6, 4090);
    CHECK(d == -12, "angle 6 -> 4090 is -12 through the wrap");
    d = r4_field_delta(ang.kind, 0x10000 + 100, 0x10000 + 200);
    CHECK(d == 100, "accumulated angles keep only the 12-bit difference");
    d = r4_field_delta(angh.kind, (int32_t)(int16_t)0xFFF0, 0x0010);
    CHECK(d == 32, "s16 angle -16 -> 16 is +32");
    d = r4_field_delta(lin.kind, 1000, 1000 + 0x1001);
    CHECK(r4_field_snaps(&lin, d), "a jump past the limit snaps");
    CHECK(!r4_field_snaps(&lin, 0x1000), "the limit itself interpolates");
    CHECK(r4_field_snaps(&ang, r4_field_delta(ang.kind, 0, 0x201)),
          "a fast turn snaps");
    CHECK(!r4_field_snaps(&ang, r4_field_delta(ang.kind, 0, 0xFFF)),
          "a small turn backwards through 0 does not snap");
}

static void test_kmh(void) {
    for (int v = -3000; v <= 3000; v++) {
        if (r4_kmh((int16_t)v) != handler_kmh((int16_t)v)) {
            char msg[64];
            snprintf(msg, sizeof msg, "kmh(%d): %d vs handler %d", v,
                     r4_kmh((int16_t)v), handler_kmh((int16_t)v));
            CHECK(0, msg);
            break;
        }
    }
}

static void test_classify(void) {
    uint32_t gp[6] = {0x3C04800Fu, 0x3C038010u, 0x8C822F94u, 0x8C63F860u,
                      0x27BDFFD8u, 0xAFBF0024u};
    uint32_t split[6] = {0x3C04800Fu, 0x3C038010u, 0x8C822F94u, 0x8C63F860u,
                         0x27BDFFC8u, 0xAFBF0034u};
    uint32_t mirror[6] = {0x27BDFFC8u, 0x3C03800Fu, 0x8C622F94u, 0x00002021u,
                          0xAFBF0030u, 0xAFB5002Cu};
    uint32_t ovl665[6] = {0x27BDFFE8u, 0x00002021u, 0, 0, 0, 0};
    CHECK(r4_classify_handler(0x8011729Cu, gp) == R4_MODE_GP, "overlay 660 GP race");
    CHECK(r4_classify_handler(0x80114C30u, split) == R4_MODE_SPLIT, "overlay 661 split");
    CHECK(r4_classify_handler(0x80114A38u, mirror) == R4_MODE_MIRROR, "overlay 659 mirror");
    CHECK(r4_classify_handler(0x80114A38u, ovl665) == R4_MODE_NONE,
          "overlay 665 at the same address is not a race");
    CHECK(r4_classify_handler(0x8011729Cu, split) == R4_MODE_NONE,
          "right address, wrong code: no");
    CHECK(r4_classify_handler(0x8005E118u, NULL) == R4_MODE_DEMO, "attract demo");
    CHECK(r4_classify_handler(0x8002A464u, NULL) == R4_MODE_REPLAY, "replay");
    CHECK(r4_classify_handler(0x80012345u, gp) == R4_MODE_NONE, "unknown handler");
}

static void test_field_table(void) {
    int seen_pos = 0, seen_yaw = 0, wheel_self = 0;
    CHECK(R4_INTERP_CAR_COUNT == 10, "ten cars");
    for (unsigned k = 1; k < 2; k++)
        CHECK(R4_INTERP_CAR_BASES[k] - R4_INTERP_CAR_BASES[k - 1] == 0x320,
              "P2 follows P1 at stride 0x320");
    for (unsigned k = 3; k < R4_INTERP_CAR_COUNT; k++)
        CHECK(R4_INTERP_CAR_BASES[k] - R4_INTERP_CAR_BASES[k - 1] == 0x320,
              "opponents at stride 0x320");
    for (unsigned f = 0; f < R4_INTERP_CAR_FIELD_COUNT; f++) {
        const R4InterpField *fd = &R4_INTERP_CAR_FIELDS[f];
        CHECK(fd->off < 0x320, "field inside the car object");
        CHECK(fd->snap > 0, "every field has a snap limit");
        if (fd->kind == R4_FIELD_ANGLE12_H)
            CHECK((fd->off & 1) == 0, "s16 fields are halfword aligned");
        else
            CHECK((fd->off & 3) == 0, "s32 fields are word aligned");
        if (fd->off == 0x10) seen_pos = 1;
        if (fd->off == 0x54 && fd->kind == R4_FIELD_ANGLE12) seen_yaw = 1;
        if (fd->off == 0x80 && fd->group == R4_GROUP_SELF) wheel_self = 1;
    }
    CHECK(seen_pos && seen_yaw, "world position and yaw are interpolated");
    CHECK(wheel_self, "wheel spin snaps alone, never the whole car");
    CHECK(R4_INTERP_CAMERA_BASE == 0x1F800000u && R4_INTERP_CAMERA_FIELD_COUNT == 6,
          "scratchpad camera: position + three angles");
}

int main(void) {
    test_lerp_and_wrap();
    test_kmh();
    test_classify();
    test_field_table();
    printf(failures ? "FAILED (%d)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
