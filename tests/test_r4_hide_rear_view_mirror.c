#include "r4_hide_rear_view_mirror.h"

#include <stdio.h>

static int failures;
#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } \
} while (0)

int main(void)
{
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
