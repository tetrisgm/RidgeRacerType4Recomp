/* r4_widescreen_pvs.h - widen R4's course visibility list for wide views.
 *
 * R4 draws the course from a precomputed list of blocks per (track section,
 * 45-degree heading octant): 0x8006EB58 copies the table entry for the
 * camera's octant into the frame list at 0x8010E3C0 (a count word, capped at
 * 255, then block pointers). Each table entry is a count word followed by u16
 * block indices; a block pointer is *(0x800F3064) + index * 0x50. A 4:3 view
 * needs about +/-51 degrees around the heading, 16:9 +/-59 and 32:9 +/-78, so
 * wide views can miss blocks at the edges. The plugin appends the blocks of
 * the neighbouring octants of the same section to the frame list before the
 * renderer consumes it. The list is rebuilt every frame, so the change is
 * transient.
 *
 * Pure helpers so tests/test_r4_widescreen.c can check them.
 */
#ifndef R4_WIDESCREEN_PVS_H
#define R4_WIDESCREEN_PVS_H

#include <stdint.h>

#define R4_PVS_LIST_ADDR       0x8010E3C0u   /* count, then pointers */
#define R4_PVS_LIST_MAX        255u
#define R4_PVS_TABLE_PTR_ADDR  0x800F39D0u   /* -> u32 table[sections][8] */
#define R4_PVS_BLOCK_BASE_ADDR 0x800F3064u   /* -> blocks, 0x50 bytes each */
#define R4_PVS_BLOCK_STRIDE    0x50u
/* One entry per heading octant: 0x8006F5AC indexes table[section * 8 +
 * octant]. (This said 9, so from section 1 on the union read the entries of
 * other sections and octants.) */
#define R4_PVS_COLUMNS         8u

/* Append each of add[0..nadd) to list[0..*count) unless already present,
 * stopping at cap. Returns how many were appended. */
static inline uint32_t r4_pvs_union(uint32_t *list, uint32_t *count,
                                    const uint32_t *add, uint32_t nadd,
                                    uint32_t cap)
{
    uint32_t appended = 0;
    for (uint32_t i = 0; i < nadd && *count < cap; i++) {
        uint32_t v = add[i], j = 0;
        while (j < *count && list[j] != v) j++;
        if (j < *count) continue;
        list[(*count)++] = v;
        appended++;
    }
    return appended;
}

/* Octants to add for a view that reveals `margin` pixels per side of a
 * 320-wide screen: the two neighbours from 16:9 up, four from about 32:9. */
static inline int r4_pvs_octant_reach(int32_t margin)
{
    if (margin <= 0) return 0;
    return margin >= 240 ? 2 : 1;
}

#endif /* R4_WIDESCREEN_PVS_H */
