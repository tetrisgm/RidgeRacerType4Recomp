/* r4_modern_rumble.h - R4's own vibration for pads without motors (pure).
 *
 * R4 runs its vibration patterns for every pad (0x8004BBC0, per port state
 * at 0x8010C2A0 + port * 24): channel A (pattern id +4, clock +8) and channel
 * B (id +6, clock +10, written last so it wins). With a DualShock (pad
 * +0x1C set) it sends entries of the tables at 0x800A0158 (A) and 0x800A0138
 * (B), index id - 1; otherwise it uses a legacy encoding at index id + 2.
 * Each table entry is { small u8, large u8, end frame s16 } with cumulative
 * end frames, closed by a negative one; the clock counts frames since the
 * pattern started. So the DualShock bytes R4 would send at any moment are the
 * first DualShock entry whose end frame is past the clock. */
#ifndef R4_MODERN_RUMBLE_H
#define R4_MODERN_RUMBLE_H

#include <stdint.h>

#define R4_RUMBLE_STATE        0x8010C2A0u
#define R4_RUMBLE_STATE_STRIDE 24u
#define R4_RUMBLE_TABLE_A      0x800A0158u
#define R4_RUMBLE_TABLE_B      0x800A0138u
#define R4_RUMBLE_TABLE_LEN    8u
#define R4_RUMBLE_OFF_FLAGS    0x800AD6E0u   /* +port*4+2: vibration off */
#define R4_RUMBLE_MAX_STEPS    32u

typedef uint32_t (*R4RumbleRead32)(uint32_t);
typedef uint16_t (*R4RumbleRead16)(uint32_t);
typedef uint8_t  (*R4RumbleRead8)(uint32_t);

typedef struct R4RumbleMotors { uint32_t small, large; } R4RumbleMotors;

static inline int r4_rumble_ram(uint32_t a) {
    return a >= 0x80000000u && a < 0x80200000u;
}

/* DualShock bytes of pattern id (1-based) of one table at clock frames. */
static inline R4RumbleMotors r4_rumble_lookup(uint32_t table, int id, int clock,
                                              R4RumbleRead32 r32,
                                              R4RumbleRead16 r16,
                                              R4RumbleRead8 r8) {
    R4RumbleMotors m = { 0u, 0u };
    uint32_t entry;
    if (id < 1 || id > (int)R4_RUMBLE_TABLE_LEN || clock < 0) return m;
    entry = r32(table + (uint32_t)(id - 1) * 4u);
    if (!r4_rumble_ram(entry)) return m;
    for (uint32_t k = 0; k < R4_RUMBLE_MAX_STEPS; ++k, entry += 4u) {
        const int end = (int16_t)r16(entry + 2u);
        if (end < 0) break;
        if (clock < end) {
            m.small = r8(entry) ? 1u : 0u;
            m.large = r8(entry + 1u);
            break;
        }
    }
    return m;
}

/* What R4 would send a DualShock on this port now. */
static inline R4RumbleMotors r4_rumble_intent(uint32_t port, R4RumbleRead32 r32,
                                              R4RumbleRead16 r16,
                                              R4RumbleRead8 r8) {
    const uint32_t st = R4_RUMBLE_STATE + port * R4_RUMBLE_STATE_STRIDE;
    const int id_a = (int16_t)r16(st + 4u), id_b = (int16_t)r16(st + 6u);
    R4RumbleMotors m = { 0u, 0u };
    if (r16(R4_RUMBLE_OFF_FLAGS + port * 4u + 2u)) return m;
    if (id_a)
        m = r4_rumble_lookup(R4_RUMBLE_TABLE_A, id_a, (int16_t)r16(st + 8u),
                             r32, r16, r8);
    if (id_b)
        m = r4_rumble_lookup(R4_RUMBLE_TABLE_B, id_b, (int16_t)r16(st + 10u),
                             r32, r16, r8);
    return m;
}

#endif /* R4_MODERN_RUMBLE_H */
