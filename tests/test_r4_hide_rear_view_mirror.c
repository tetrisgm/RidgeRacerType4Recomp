#include "r4_hide_rear_view_mirror.h"

#include <stdio.h>

static int failures;
#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } \
} while (0)

/* Effective address of a retail lui/sw pair: (lui imm << 16) + signed offset. */
static uint32_t lui_sw_address(uint32_t lui_word, uint32_t sw_word)
{
    return (uint32_t)((lui_word & 0xFFFFu) << 16) + (uint32_t)(int32_t)(int16_t)(sw_word & 0xFFFFu);
}

int main(void)
{
    /* Retail words at 0x80070A6C (lui v1,0x8011) and 0x80070A74 (sw v0,-0x17a0(v1)). */
    CHECK(lui_sw_address(0x3C038011u, 0xAC62E860u) == R4_REAR_MIRROR_GATE,
          "gate matches the word the retail entry stores to");
    R4RearMirrorEntryState state = { 123u, 0xA5A5A5A5u };
    CHECK(!r4_rear_mirror_suppress_entry(0, R4_REAR_MIRROR_ENTRY, &state),
          "disabled package does not alter the entry state");
    CHECK(state.argument == 123u && state.gate_value == 0xA5A5A5A5u,
          "disabled state stays untouched");
    CHECK(!r4_rear_mirror_suppress_entry(1, 0x80070A5Cu, &state),
          "unrelated function entry is untouched");
    CHECK(!r4_rear_mirror_suppress_entry(1, R4_REAR_MIRROR_ENTRY, NULL),
          "missing CPU state is rejected");
    CHECK(r4_rear_mirror_suppress_entry(1, R4_REAR_MIRROR_ENTRY, &state),
          "enabled mirror entry uses the stock no-render branch");
    CHECK(state.argument == R4_REAR_MIRROR_EARLY_RETURN_ARG && state.gate_value == 0,
          "sub-threshold argument leaves the gate clear and returns through the epilogue");
    if (failures) return 1;
    puts("rear-view mirror hook policy: ok");
    return 0;
}
