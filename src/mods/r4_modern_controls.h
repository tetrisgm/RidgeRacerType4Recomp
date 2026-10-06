/* r4_modern_controls.h - the Modern scheme's race mapping, pure (no RAM).
 * r4_modern_controls.c applies it only during a race. */
#ifndef R4_MODERN_CONTROLS_H
#define R4_MODERN_CONTROLS_H

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

#define R4_MC_PLAYERS 2u
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

/* What the Modern buttons press on R4's NeGcon, from R4's own NeGcon button
 * config (so an in-game remap is honoured) as SIO masks, plus R4's NeGcon
 * calibration: the pressure rest offsets and the twist dead zone and range
 * (R4 steers on |twist - 128| past the dead zone, at full lock at dead zone +
 * range). Defaults: D-pad Down shifts up, Up shifts down, B (the Triangle
 * bit) changes the camera view; dead zone 6, range 38. */
typedef struct R4ModernNegcon {
    uint32_t upshift, downshift, view;   /* SIO masks */
    uint32_t rest_i, rest_ii;            /* pressure rest offsets */
    uint32_t twist_deadzone, twist_range;
} R4ModernNegcon;

/* R4's config halfword is byte-swapped against the SIO word. */
static inline uint32_t r4_mc_cfg_to_sio(uint32_t cfg, uint32_t fallback) {
    const uint32_t sio = ((cfg & 0xFFu) << 8) | ((cfg >> 8) & 0xFFu);
    return sio ? sio : fallback;
}

static inline uint32_t r4_mc_pressure(uint32_t trigger, uint32_t rest) {
    const uint32_t v = rest + (trigger * R4_MC_NEGCON_FULL + 127u) / 255u;
    return v > 255u ? 255u : v;
}

/* Stick X (0x80 centre) onto R4's twist so the whole stick travel spans R4's
 * steering: any deflection clears R4's dead zone, a full one is full lock. */
static inline uint32_t r4_mc_twist(uint32_t lx, const R4ModernNegcon *ng) {
    const int d = (int)lx - 128;
    const uint32_t m = d < -127 ? 127u : (uint32_t)(d < 0 ? -d : d);
    uint32_t span;
    if (!m) return 0x80u;
    span = ng->twist_deadzone + (m * ng->twist_range + 126u) / 127u;
    if (span > 127u) span = 127u;
    return d < 0 ? 128u - span : 128u + span;
}

/* Race mapping for one frame. Returns 0 (pass through) for keyboards, pads
 * without both trigger axes and wheels; else fills *out with a NeGcon frame
 * and returns 1. */
static inline int r4_modern_controls_map(const PSXModPadFrame *frame,
                                         const R4ModernNegcon *ng,
                                         PSXModPadOutput *out) {
    uint32_t in, mapped, twist;
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

    /* Twist is the left stick; a digital-mode pad (stick folded onto the
     * D-pad) steers at full lock. */
    if (frame->type == PSX_MOD_PAD_DUALSHOCK) {
        twist = r4_mc_twist(frame->lx, ng);
    } else if (r4_mc_pressed(in, R4_MC_PAD_LEFT) &&
               !r4_mc_pressed(in, R4_MC_PAD_RIGHT)) {
        twist = 0x00u;
    } else if (r4_mc_pressed(in, R4_MC_PAD_RIGHT) &&
               !r4_mc_pressed(in, R4_MC_PAD_LEFT)) {
        twist = 0xFFu;
    } else {
        twist = 0x80u;
    }

    out->buttons = mapped & 0xFFFFu;
    out->type = PSX_MOD_PAD_NEGCON;
    out->lx = twist;
    out->ly = out->rx = out->ry = 0x80u;
    out->negcon_i = r4_mc_pressure(frame->host_rt, ng->rest_i);   /* gas */
    out->negcon_ii = r4_mc_pressure(frame->host_lt, ng->rest_ii); /* brake */
    out->negcon_l = 0u;
    return 1;
}

#endif /* R4_MODERN_CONTROLS_H */
