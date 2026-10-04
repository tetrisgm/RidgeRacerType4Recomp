/* Pure entry-hook policy for r4.enhancement.hide-rear-view-mirror. */
#ifndef R4_HIDE_REAR_VIEW_MIRROR_H
#define R4_HIDE_REAR_VIEW_MIRROR_H

#include <stdint.h>

#define R4_REAR_MIRROR_ENTRY       0x80070A58u
#define R4_REAR_MIRROR_GATE        0x8011E860u
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
