/* r4_modern_controls.h - the Modern scheme's race mapping, pure (no RAM).
 * r4_modern_controls.c applies it only during a race. */
#ifndef R4_MODERN_CONTROLS_H
#define R4_MODERN_CONTROLS_H

#include <math.h>
#include <stdint.h>

#include "mod_plugins.h"

/* PS1 pad word, active low: a zero bit is a pressed button. */
#define R4_MC_PAD_UP       0x0010u
#define R4_MC_PAD_DOWN     0x0040u
#define R4_MC_PAD_LEFT     0x0080u
#define R4_MC_PAD_RIGHT    0x0020u
#define R4_MC_PAD_L2       0x0100u
#define R4_MC_PAD_R2       0x0200u
#define R4_MC_PAD_L1       0x0400u
#define R4_MC_PAD_R1       0x0800u
#define R4_MC_PAD_TRIANGLE 0x1000u
#define R4_MC_PAD_CIRCLE   0x2000u
#define R4_MC_PAD_CROSS    0x4000u
#define R4_MC_PAD_SQUARE   0x8000u
#define R4_MC_PAD_FACE_SHOULDERS 0xFF00u
#define R4_MC_PAD_DPAD     0x00F0u

/* Every pad the runtime can present: two offline ports, four Link Battle
 * seats online. R4 keeps NeGcon config and calibration for its two ports
 * only; seats 3-4 use port 1's (R4_MC_CONFIG_PORT). */
#define R4_MC_PLAYERS 4u
#define R4_MC_CONFIG_PORT(player) ((player) < 2u ? (player) : 0u)
#define R4_MC_HOST_TRIGGERS \
    (PSX_MOD_PAD_HOST_GAMEPAD | PSX_MOD_PAD_HOST_LT | PSX_MOD_PAD_HOST_RT)
#define R4_MC_ALLOWED_TYPES \
    (PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DIGITAL) | \
     PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DUALSHOCK) | \
     PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_NEGCON))

int r4_modern_controls_scheme_is_classic(const char *value);

static inline int r4_mc_pressed(uint32_t buttons, uint32_t mask) {
    return (buttons & mask) == 0;
}

/* R4's NeGcon in a race: its pressure channels read as calibrated 0..106
 * (raw minus the per-port rest offset, clamped at 106), so the triggers are
 * scaled onto that span and the whole travel stays proportional. */
#define R4_MC_NEGCON_FULL 106u

/* Response tuning (hidden mod options, r4.modern-controls/controls-tuning;
 * docs/MODERN_CONTROLS.md). Percentages unless noted. Defaults aim at a
 * forgiving, Forza-Horizon-like arcade feel. */
typedef struct R4ModernTuning {
    uint32_t trigger_deadzone;    /* inner dead zone, % of travel: 2 */
    uint32_t trigger_full;        /* travel that counts as full, %: 95 */
    uint32_t trigger_gamma;       /* ease-in exponent x100: 130 */
    uint32_t throttle_low_share;  /* request share fed through R4's feather
                                     band I 0..52, %: 40 (measured) */
    uint32_t throttle_step_share; /* request share where R4's step lands
                                     (I 53), %: 70 (measured) */
    uint32_t launch_speed;        /* below launch_speed + request x
                                     launch_span (R4 speed units) any press
                                     holds at least the step, so the car
                                     leaves rest and a light press settles
                                     at a gentle cruise: 20 */
    uint32_t launch_span;         /* 300 */
    uint32_t feather_min;         /* feather band floor, NeGcon pressure:
                                     R4 roughly holds speed here: 32 */
    uint32_t brake_min;           /* brake floor for any press past the dead
                                     zone, NeGcon pressure: 6 */
    uint32_t stick_deadzone;      /* radial dead zone, %: 6 */
    uint32_t stick_ease;          /* centre ease, % quadratic blend: 20 */
    uint32_t steer_speed_cut;     /* lock removed at high speed, %: 10 (20
                                     made drifts measurably harder to hold) */
    uint32_t steer_speed_low;     /* R4 speed where the cut starts: 450 */
    uint32_t steer_speed_high;    /* R4 speed where it is complete: 900 */
} R4ModernTuning;

#define R4_MC_TUNING_DEFAULTS \
    { 2u, 95u, 130u, 40u, 70u, 20u, 300u, 32u, 6u, 6u, 20u, 10u, 450u, 900u }

/* What the Modern buttons press on R4's NeGcon, from R4's own NeGcon button
 * config (so an in-game remap is honoured) as SIO masks, plus R4's NeGcon
 * calibration: the pressure rest offsets and the twist dead zone and range
 * (R4 steers on |twist - 128| past the dead zone, at full lock at dead zone +
 * range). Defaults: D-pad Down shifts up, Up shifts down, B (the Triangle
 * bit) changes the camera view; dead zone 6, range 38. speed and phase are
 * guest RAM read at pad time (the car's speed, R4's 30 Hz frame parity), so
 * the mapping stays a pure function of the pad and guest state. */
typedef struct R4ModernNegcon {
    uint32_t upshift, downshift, view;   /* SIO masks */
    uint32_t rest_i, rest_ii;            /* pressure rest offsets */
    uint32_t twist_deadzone, twist_range;
    uint32_t speed;                      /* car speed, 0 when unknown */
    uint32_t phase;                      /* R4 logic frame counter */
    R4ModernTuning tuning;
} R4ModernNegcon;

/* R4's config halfword is byte-swapped against the SIO word. */
static inline uint32_t r4_mc_cfg_to_sio(uint32_t cfg, uint32_t fallback) {
    const uint32_t sio = ((cfg & 0xFFu) << 8) | ((cfg >> 8) & 0xFFu);
    return sio ? sio : fallback;
}

static inline float r4_mc_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Trigger 0..255 -> 0..1: inner dead zone, outer "full" point, ease-in. */
static inline float r4_mc_trigger_curve(uint32_t trigger, const R4ModernTuning *t) {
    const float dz = (float)t->trigger_deadzone / 100.0f;
    const float full = (float)t->trigger_full / 100.0f;
    const float p = (float)(trigger > 255u ? 255u : trigger) / 255.0f;
    float q;
    if (full <= dz) return p > dz ? 1.0f : 0.0f;
    q = r4_mc_clamp01((p - dz) / (full - dz));
    if (q <= 0.0f) return 0.0f;
    return powf(q, (float)t->trigger_gamma / 100.0f);
}

static inline uint32_t r4_mc_pressure_level(float a, uint32_t rest) {
    const uint32_t v = rest + (uint32_t)(a * (float)R4_MC_NEGCON_FULL + 0.5f);
    return v > 255u ? 255u : v;
}

/* R4 has a throttle step at half pressure (I = 53 of 106). Measured from a
 * race savestate: from rest the car only moves while I >= 53 is held (any
 * frame below resets the launch, so dithering cannot start it); rolling, I
 * 0..52 is a graded feather (R4 roughly holds speed near 32) up to about 40% of the available
 * acceleration, the step jumps to about 70%, and 53..106 rises to full.
 * So: below a launch speed that grows with the request any press holds at
 * least the step (scaled 53..106 with the request), so a light press rolls
 * the car away and settles at a gentle cruise; rolling,
 * the request maps onto the three bands, with requests inside the step's gap
 * alternating 52 and 53 on R4's 30 Hz frames in an ordered 16-frame
 * pattern so the average force stays proportional. */
#define R4_MC_THROTTLE_STEP ((R4_MC_NEGCON_FULL + 1u) / 2u)
static inline uint32_t r4_mc_throttle(float a, const R4ModernNegcon *ng) {
    static const uint8_t bayer16[16] = {
        0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
    const R4ModernTuning *t = &ng->tuning;
    const float low = (float)t->throttle_low_share / 100.0f;
    const float high = (float)t->throttle_step_share / 100.0f;
    const float step = (float)R4_MC_THROTTLE_STEP;
    const float full = (float)R4_MC_NEGCON_FULL;
    const float launch = (float)t->launch_speed + a * (float)t->launch_span;
    const float fmin = t->feather_min < R4_MC_THROTTLE_STEP
        ? (float)t->feather_min : 0.0f;
    float level;
    if (a <= 0.0f) return ng->rest_i;
    if ((float)ng->speed < launch || !(low > 0.0f && low <= high && high < 1.0f)) {
        level = (float)ng->speed < launch ? step + a * (full - step) : a * full;
    } else if (a <= low) {
        level = fmin + a / low * (step - 1.0f - fmin);
    } else if (a < high) {
        uint32_t duty = (uint32_t)((a - low) / (high - low) * 16.0f + 0.5f);
        level = bayer16[ng->phase & 15u] < duty ? step : step - 1.0f;
    } else {
        level = step + (a - high) / (1.0f - high) * (full - step);
    }
    return r4_mc_pressure_level(level / full, ng->rest_i);
}

/* Kept for callers that want the plain linear scale. */
static inline uint32_t r4_mc_pressure(uint32_t trigger, uint32_t rest) {
    const uint32_t v = rest + (trigger * R4_MC_NEGCON_FULL + 127u) / 255u;
    return v > 255u ? 255u : v;
}

/* Left stick -> R4 twist. Radial dead zone over both axes, linear with a
 * mild centre ease, lock reduced with speed (FH "Normal"-style), then spread
 * over R4's twist so any steer clears R4's own dead zone. */
static inline uint32_t r4_mc_twist_stick(uint32_t lx, uint32_t ly,
                                         const R4ModernNegcon *ng) {
    const R4ModernTuning *t = &ng->tuning;
    const float dx = (float)((int)lx - 128) / 127.0f;
    const float dy = (float)((int)ly - 128) / 127.0f;
    const float mag = sqrtf(dx * dx + dy * dy);
    const float dz = (float)t->stick_deadzone / 100.0f;
    const float ease = (float)t->stick_ease / 100.0f;
    float s, m, k = 1.0f;
    uint32_t span;
    if (mag <= dz || dx == 0.0f) return 0x80u;
    m = r4_mc_clamp01((mag - dz) / (1.0f - dz));
    s = r4_mc_clamp01(fabsf(dx) / mag * m);
    s = (1.0f - ease) * s + ease * s * s;
    if (t->steer_speed_cut && t->steer_speed_high > t->steer_speed_low) {
        const float v = r4_mc_clamp01(
            ((float)ng->speed - (float)t->steer_speed_low) /
            (float)(t->steer_speed_high - t->steer_speed_low));
        k = 1.0f - v * (float)t->steer_speed_cut / 100.0f;
    }
    s *= k;
    if (s <= 0.0f) return 0x80u;
    span = ng->twist_deadzone + (uint32_t)ceilf(s * (float)ng->twist_range);
    if (span > 127u) span = 127u;
    return dx < 0.0f ? 128u - span : 128u + span;
}

/* Race mapping for one frame. Returns 0 (pass through) for keyboards, pads
 * without both trigger axes and wheels; else fills *out with a NeGcon frame
 * and returns 1. */
static inline int r4_modern_controls_map(const PSXModPadFrame *frame,
                                         const R4ModernNegcon *ng,
                                         PSXModPadOutput *out) {
    uint32_t in, mapped;
    if (!frame || !ng || !out) return 0;
    if ((frame->host_flags & R4_MC_HOST_TRIGGERS) != R4_MC_HOST_TRIGGERS)
        return 0;
    if (frame->type != PSX_MOD_PAD_DUALSHOCK &&
        frame->type != PSX_MOD_PAD_DIGITAL)
        return 0;

    in = frame->buttons;
    /* Face, shoulder and D-pad bits start released (R4's NeGcon shifts on
     * the D-pad); only the remapped ones are pressed again. Start, Select,
     * L3 and R3 pass. */
    mapped = in | R4_MC_PAD_FACE_SHOULDERS | R4_MC_PAD_DPAD;
    if (r4_mc_pressed(in, R4_MC_PAD_SQUARE)) mapped &= ~ng->downshift;
    if (r4_mc_pressed(in, R4_MC_PAD_CIRCLE)) mapped &= ~ng->upshift;
    if (r4_mc_pressed(in, R4_MC_PAD_R1)) mapped &= ~ng->view;

    out->buttons = mapped & 0xFFFFu;
    out->type = PSX_MOD_PAD_NEGCON;
    /* A digital-presented pad's frame is its host pad (psxrecomp gives the
     * transform the real sticks), so it steers proportionally too. */
    out->lx = r4_mc_twist_stick(frame->lx, frame->ly, ng);
    out->ly = out->rx = out->ry = 0x80u;
    out->negcon_i = r4_mc_throttle(
        r4_mc_trigger_curve(frame->host_rt, &ng->tuning), ng);          /* gas */
    {   /* brake: R4's brake is already graded; any press gets a small floor */
        const float b = r4_mc_trigger_curve(frame->host_lt, &ng->tuning);
        const float bmin = (float)(ng->tuning.brake_min < R4_MC_NEGCON_FULL
                                   ? ng->tuning.brake_min : 0u);
        out->negcon_ii = b > 0.0f
            ? r4_mc_pressure_level((bmin + b * ((float)R4_MC_NEGCON_FULL - bmin)) /
                                   (float)R4_MC_NEGCON_FULL, ng->rest_ii)
            : ng->rest_ii;
    }
    out->negcon_l = 0u;
    return 1;
}

#endif /* R4_MODERN_CONTROLS_H */
