/* Pure entry-hook policy for r4.enhancement.hide-rear-view-mirror. */
#ifndef R4_HIDE_REAR_VIEW_MIRROR_H
#define R4_HIDE_REAR_VIEW_MIRROR_H

#include <stdint.h>

#define R4_REAR_MIRROR_ENTRY       0x80070A58u
/* lui v1,0x8011 ; sw v0,-0x17a0(v1) at 0x80070A6C/0x80070A74:
 * 0x80110000 - 0x17a0 = 0x8010E860. The race-setup routine at 0x80070764
 * clears the same word, and 0x80070788 reads it. */
#define R4_REAR_MIRROR_GATE        0x8010E860u
#define R4_REAR_MIRROR_EARLY_RETURN_ARG 0u

typedef struct {
    uint32_t argument;
    uint32_t gate_value;
} R4RearMirrorEntryState;

/* The retail entry sets its gate only when a0 >= 361, then returns through
 * the ordinary epilogue whenever the gate is zero. Keep the argument below
 * that threshold and clear the gate so the stock early-return branch wins. */
static inline int r4_rear_mirror_suppress_entry(int enabled, uint32_t address,
                                                R4RearMirrorEntryState *state)
{
    if (!enabled || address != R4_REAR_MIRROR_ENTRY || state == 0) return 0;
    state->argument = R4_REAR_MIRROR_EARLY_RETURN_ARG;
    state->gate_value = 0;
    return 1;
}

#endif
