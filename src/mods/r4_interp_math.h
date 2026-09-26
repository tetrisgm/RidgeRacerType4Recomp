/* r4_interp_math.h - pure math of R4's interpolated frames (r4_interp.c).
 * Header-only so tests/test_r4_interp.c checks exactly what the plugin runs. */
#ifndef R4_INTERP_MATH_H
#define R4_INTERP_MATH_H

#include <stdint.h>

#include "r4_interp_fields.h"

/* Per-tick change of a field, wrap-aware for 12-bit angles (the game's
 * 0..4095 turn; the cheat's `sll/sra 0x14`). s16 angles are compared as s16. */
static inline int32_t r4_field_delta(uint8_t kind, int32_t prev, int32_t cur) {
    int32_t d;
    if (kind == R4_FIELD_ANGLE12_H)
        d = (int32_t)(int16_t)cur - (int32_t)(int16_t)prev;
    else
        d = (int32_t)((uint32_t)cur - (uint32_t)prev);
    if (kind == R4_FIELD_ANGLE12 || kind == R4_FIELD_ANGLE12_H)
        d = (int32_t)((uint32_t)d << 20) >> 20;
    return d;
}

static inline int r4_field_snaps(const R4InterpField *f, int32_t d) {
    int64_t a = d < 0 ? -(int64_t)d : (int64_t)d;
    return a > (int64_t)f->snap;
}

/* prev + d * alpha, alpha in Q16, rounded to nearest (int64: |d| * alpha
 * reaches 2^34 for the fine-position fields). */
static inline int32_t r4_lerp(int32_t prev, int32_t d, uint32_t alpha_q16) {
    int64_t step = ((int64_t)d * (int64_t)alpha_q16 + 0x8000) >> 16;
    return (int32_t)((uint32_t)prev + (uint32_t)step);
}

/* The HUD speed in km/h exactly as the race handlers compute it:
 * (s16)car[0x1D8] * 160 / 1168, truncated toward zero (their mulhi 0xE070381D
 * sequence at 0x80117790-0x801177C4). */
static inline int32_t r4_kmh(int16_t raw) {
    return ((int32_t)raw * 160) / 1168;
}

/* Race handlers the plugin knows how to redraw (fps_r4 notes, section 2.1). */
enum {
    R4_MODE_NONE = 0,
    /* Identified at runtime (handler address at the VSync(0) hook): */
    R4_MODE_TIME_ATTACK = 1,  /* Time Attack race, overlay 660 */
    R4_MODE_GRAND_PRIX = 2,   /* Grand Prix race (rear-view mirror, time limit), overlay 659 */
    R4_MODE_SPLIT = 3,        /* VS 2P split screen, overlay 661 (not reached in a run) */
    R4_MODE_DEMO = 4,         /* attract / music demo, EXE */
    R4_MODE_REPLAY = 5        /* replay after a Time Attack goal, EXE */
};

#define R4_SIG_WORDS 6u
typedef struct R4ModeSignature {
    uint32_t handler;
    int mode;
    uint32_t words[R4_SIG_WORDS];   /* 0 = no signature (EXE code) */
} R4ModeSignature;

/* Overlay handlers share load addresses with other overlays, so the first
 * words of the resident code identify which one is loaded. */
static const R4ModeSignature R4_MODE_SIGNATURES[] = {
    { 0x8011729Cu, R4_MODE_TIME_ATTACK,
      { 0x3C04800Fu, 0x3C038010u, 0x8C822F94u, 0x8C63F860u, 0x27BDFFD8u, 0xAFBF0024u } },
    { 0x80114A38u, R4_MODE_GRAND_PRIX,
      { 0x27BDFFC8u, 0x3C03800Fu, 0x8C622F94u, 0x00002021u, 0xAFBF0030u, 0xAFB5002Cu } },
    { 0x80114C30u, R4_MODE_SPLIT,
      { 0x3C04800Fu, 0x3C038010u, 0x8C822F94u, 0x8C63F860u, 0x27BDFFC8u, 0xAFBF0034u } },
    { 0x8005E118u, R4_MODE_DEMO,   { 0, 0, 0, 0, 0, 0 } },
    { 0x8002A464u, R4_MODE_REPLAY, { 0, 0, 0, 0, 0, 0 } },
};

/* Mode of a handler given its first R4_SIG_WORDS code words (NONE when the
 * address is unknown or the resident code does not match). */
static inline int r4_classify_handler(uint32_t handler, const uint32_t *words) {
    for (unsigned i = 0; i < sizeof R4_MODE_SIGNATURES / sizeof R4_MODE_SIGNATURES[0]; i++) {
        const R4ModeSignature *s = &R4_MODE_SIGNATURES[i];
        if (s->handler != handler) continue;
        if (s->words[0] == 0) return s->mode;
        for (unsigned w = 0; w < R4_SIG_WORDS; w++)
            if (!words || words[w] != s->words[w]) return R4_MODE_NONE;
        return s->mode;
    }
    return R4_MODE_NONE;
}

#endif
