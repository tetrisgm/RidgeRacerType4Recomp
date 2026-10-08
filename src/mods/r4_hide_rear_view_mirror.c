/* Hide R4's dedicated rear-view render pass as a presentation-only mod. */
#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_hide_rear_view_mirror.h"

#include <stdio.h>

#define PLUGIN_ID "r4.hide-rear-view-mirror"

static int s_enabled;
static int s_hook_registered;

static void r4_hide_rear_mirror_entry(CPUState *cpu, uint32_t address)
{
    R4RearMirrorEntryState state;
    if (cpu == NULL) return;
    state.argument = cpu->gpr[4];
    state.gate_value = psx_mod_read_word(R4_REAR_MIRROR_GATE);
    if (!r4_rear_mirror_suppress_entry(s_enabled, address, &state)) return;

    cpu->gpr[4] = state.argument;
    psx_mod_write_word(R4_REAR_MIRROR_GATE, state.gate_value);
}

static void r4_hide_rear_mirror_activate(void)
{
    s_enabled = 1;
    if (s_hook_registered != 1)
        fprintf(stderr, "[r4-hide-mirror] render hook unavailable\n");
}

PSX_MOD_CONSTRUCTOR(r4_register_hide_rear_view_mirror)
{
    s_hook_registered += psx_mod_register_function_entry_plugin(
        PLUGIN_ID, R4_REAR_MIRROR_ENTRY, r4_hide_rear_mirror_entry);
    (void)psx_mod_register_activation_plugin(PLUGIN_ID,
                                              r4_hide_rear_mirror_activate);
}
