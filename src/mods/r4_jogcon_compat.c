/* R4 JogCon input compatibility.
 *
 * R4 recognizes JogCon ID E3 and parses its digital buttons and calibrated
 * wheel position. Its per-frame input translator, however, handles internal
 * pad mode 0 (digital) and 1 (DualShock analog), but sends mode 2 (JogCon) to
 * a path that emits no game controls. At the translator entry, switch only
 * R4's internal mode field to the existing analog-control path. The guest's
 * SIO identity and the decoded wheel/button fields remain untouched.
 */
#include "mod_plugins.h"
#include "cpu_state.h"

#define R4_JOGCON_PLUGIN  "r4.jogcon-input"
#define R4_INPUT_UPDATE   0x8002961Cu
#define R4_PAD0_BASE      0x800F3BE8u
#define R4_PAD0_TYPE      (R4_PAD0_BASE + 1u)
#define R4_PAD0_MODE      (R4_PAD0_BASE + 16u)
#define R4_JOGCON_ID      0xE3u
#define R4_JOGCON_MODE    2u
#define R4_ANALOG_MODE    1u

static void r4_jogcon_input_compat(CPUState *cpu, uint32_t address) {
    (void)cpu;
    if (address != R4_INPUT_UPDATE) return;
    if (psx_mod_read_byte(R4_PAD0_TYPE) != R4_JOGCON_ID) return;
    if (psx_mod_read_word(R4_PAD0_MODE) != R4_JOGCON_MODE) return;
    psx_mod_write_word(R4_PAD0_MODE, R4_ANALOG_MODE);
}

PSX_MOD_CONSTRUCTOR(r4_jogcon_compat_register) {
    (void)psx_mod_register_function_entry_plugin(
        R4_JOGCON_PLUGIN, R4_INPUT_UPDATE, r4_jogcon_input_compat);
}
