/* r4_modern_rumble.c - "Rumble" for R4 Controls (r4.modern-controls).
 *
 * Modern presents a NeGcon in races, which has no motors, so R4's vibration
 * patterns (crashes, wall scrapes, ...) never reach a pad. Every frame, on
 * R4's input update, this reads R4's live pattern state and hands the
 * runtime the DualShock motor values R4 would have sent
 * (psx_mod_set_host_rumble; r4_modern_rumble.h). Only for ports presenting
 * a NeGcon (ID 0x23) and not in DualShock mode, so Classic's real DualShock
 * rumble is untouched; R4's own vibration off setting is honoured. Host
 * output only: it never writes guest state, so online it stays on per player
 * (netplay = "host_output"); it reports both ports and psxrecomp keeps only
 * this peer's own port, outside rollback resims. */
#include <stdint.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_modern_rumble.h"

#define R4_RUMBLE_PLUGIN   "r4.modern-rumble"
#define R4_RUMBLE_HOOK     0x8002961Cu   /* R4's per-frame input update */
#define R4_RUMBLE_PAD0     0x800F3BE8u
#define R4_RUMBLE_PAD_SIZE 0x5Cu
#define R4_RUMBLE_NEGCON   0x23u

static uint32_t r4_rumble_r32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t r4_rumble_r16(uint32_t a) { return psx_mod_read_half(a); }
static uint8_t  r4_rumble_r8(uint32_t a)  { return psx_mod_read_byte(a); }

static void r4_modern_rumble_update(CPUState *cpu, uint32_t address) {
    (void)cpu;
    if (address != R4_RUMBLE_HOOK) return;
    for (uint32_t port = 0; port < 2u; ++port) {
        const uint32_t pad = R4_RUMBLE_PAD0 + port * R4_RUMBLE_PAD_SIZE;
        R4RumbleMotors m;
        if (psx_mod_read_byte(pad + 1u) != R4_RUMBLE_NEGCON ||
            psx_mod_read_half(pad + 0x1Cu))
            continue;
        m = r4_rumble_intent(port, r4_rumble_r32, r4_rumble_r16, r4_rumble_r8);
        (void)psx_mod_set_host_rumble(port, m.small, m.large);
    }
}

PSX_MOD_CONSTRUCTOR(r4_modern_rumble_register) {
    (void)psx_mod_register_function_entry_plugin(
        R4_RUMBLE_PLUGIN, R4_RUMBLE_HOOK, r4_modern_rumble_update);
}
