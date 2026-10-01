/* r4_pvs.h - R4's course visibility list, shared by the widescreen and
 * Max Detail plugins.
 *
 * R4 draws the course from a precomputed list of blocks per (track section,
 * 45-degree heading octant): 0x8006EB58 copies the table entry for the
 * camera's section and octant (table[section * 8 + octant]; a section is
 * five track segments) into the frame list at 0x8010E3C0 (a count
 * word, capped at 255, then block pointers). Each table entry is a count word
 * followed by u16 block indices; a block pointer is *(0x800F3064) + index *
 * 0x50. The lists are short (6 to 21 blocks) and tuned to a 4:3 view at the
 * stock draw distance.
 *
 *   - Wide views (r4.enhancement.widescreen) need more heading: a 4:3 view
 *     covers about +/-51 degrees around the heading, 16:9 +/-59 and 32:9
 *     +/-78, so the neighbouring octants of the same section are added.
 *   - A longer draw distance (r4.enhancement.max-detail) needs more road
 *     ahead and behind: the neighbouring sections' lists are added.
 *
 * Both plugins hook the first consumer of the list and call r4_pvs_merge(),
 * which appends every (section, octant) within the requested reach. The
 * union is idempotent, so with both plugins active the list is the full
 * cross product whichever hook runs first. The game rebuilds the list every
 * frame, so the change is transient.
 *
 * Pure helpers so tests/ can check them.
 */
#ifndef R4_PVS_H
#define R4_PVS_H

#include <stdint.h>

#define R4_PVS_LIST_ADDR       0x8010E3C0u   /* count, then pointers */
#define R4_PVS_LIST_MAX        255u
#define R4_PVS_TABLE_PTR_ADDR  0x800F39D0u   /* -> u32 table[sections][8] */
#define R4_PVS_BLOCK_BASE_ADDR 0x800F3064u   /* -> blocks, 0x50 bytes each */
#define R4_PVS_BLOCK_STRIDE    0x50u
/* One entry per heading octant: 0x8006F5AC indexes table[section * 8 +
 * octant]. (Before R4 Max Detail this said 9, so the widescreen union read
 * the entries of other sections and octants.) */
#define R4_PVS_COLUMNS         8u
/* Track segments of the loaded course; a section is five segments
 * (0x8006F510 divides the segment index by 5; 0x80026A30 wraps segments
 * modulo this count). */
#define R4_TRACK_SEGMENT_COUNT_ADDR 0x800AC084u
#define R4_PVS_SECTIONS_MAX    1024u

/* Hook points both plugins use. The lookup at 0x8006F5AC calls octant()
 * with the section already in s0 (set in the jal's delay slot) and then
 * indexes the table at R4_PVS_OCTANT_RA; 0x8007166C is the list's first
 * consumer on the two main course draw paths (the mirror does not call it).
 * tools/r4_detail_scan.py --check holds every value here to the game's code. */
#define R4_PVS_OCTANT_FN    0x8006F584u   /* octant(a0 = camera) */
#define R4_PVS_OCTANT_RA    0x8006F5D8u   /* return into the list lookup */
#define R4_PVS_MERGE_FN     0x8007166Cu   /* first consumer of the list */
#define R4_PVS_MERGE_RA1    0x8006F02Cu   /* course draw, path 1 */
#define R4_PVS_MERGE_RA2    0x8006F090u   /* course draw, path 2 */

/* The camera's heading octant from its yaw word (a0 + 0x14), exactly as
 * octant() computes it. */
static inline uint32_t r4_pvs_octant(uint32_t yaw)
{
    return (((yaw & 0xFFFu) + 0x100u) >> 9) & 7u;
}

typedef uint32_t (*R4PvsRead32)(uint32_t address);
typedef uint16_t (*R4PvsRead16)(uint32_t address);
typedef void (*R4PvsWrite32)(uint32_t address, uint32_t value);

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

/* Sections of the loaded course, or 0 when the segment count is not
 * plausible (no course loaded). */
static inline uint32_t r4_pvs_section_count(R4PvsRead32 rd32)
{
    uint32_t segments = rd32(R4_TRACK_SEGMENT_COUNT_ADDR);
    if (segments == 0u || segments > 5u * R4_PVS_SECTIONS_MAX) return 0u;
    return (segments + 4u) / 5u;
}

/* `section` moved by `delta` around a circuit of `nsec` sections. */
static inline uint32_t r4_pvs_section_step(uint32_t section, int delta, uint32_t nsec)
{
    int64_t s = ((int64_t)section + delta) % (int64_t)nsec;
    return (uint32_t)(s < 0 ? s + (int64_t)nsec : s);
}

typedef struct {
    int ok;                   /* the guest's table and list looked valid */
    uint32_t before, after;   /* list length on entry and on exit */
} R4PvsMerge;

/* Append to the frame list the blocks of every (section +/- sd, octant +/-
 * od) with sd <= sec_reach and od <= oct_reach, except the camera's own
 * entry, which the list already holds. Nearest first: sections 0, -1, +1,
 * -2, +2 ..., and within each, octants in the same order, so the 255-entry
 * cap drops the farthest additions. With sec_reach 0 this is exactly the
 * widescreen octant union. Sections wrap around the circuit; a section
 * reach needs a plausible `nsec` (r4_pvs_section_count) and is dropped
 * without one. Nothing is written when the guest's table or list does not
 * look valid. */
static inline R4PvsMerge r4_pvs_merge(R4PvsRead32 rd32, R4PvsRead16 rd16,
                                      R4PvsWrite32 wr32, uint32_t section,
                                      uint32_t octant, uint32_t nsec,
                                      int sec_reach, int oct_reach)
{
    R4PvsMerge r = { 0, 0u, 0u };
    if (sec_reach < 0) sec_reach = 0;
    if (oct_reach < 0) oct_reach = 0;
    if (oct_reach > 4) oct_reach = 4;
    if (nsec == 0u || section >= nsec) sec_reach = 0;
    if (2 * sec_reach + 1 > (int)nsec && nsec != 0u) sec_reach = ((int)nsec - 1) / 2;
    uint32_t table = rd32(R4_PVS_TABLE_PTR_ADDR);
    uint32_t blocks = rd32(R4_PVS_BLOCK_BASE_ADDR);
    if ((table & 0xFFE00003u) != 0x80000000u || (blocks & 0xFFE00000u) != 0x80000000u)
        return r;
    uint32_t list[R4_PVS_LIST_MAX], count = rd32(R4_PVS_LIST_ADDR);
    if (count > R4_PVS_LIST_MAX) return r;
    for (uint32_t i = 0; i < count; i++) list[i] = rd32(R4_PVS_LIST_ADDR + 4u + 4u * i);
    r.ok = 1;
    r.before = r.after = count;
    if (sec_reach == 0 && oct_reach == 0) return r;
    for (int sd = 0; sd <= sec_reach; sd++) {
        for (int ss = sd ? -1 : 1; ss <= 1; ss += 2) {
            uint32_t sec = sd ? r4_pvs_section_step(section, ss * sd, nsec) : section;
            for (int od = 0; od <= oct_reach; od++) {
                for (int os = od ? -1 : 1; os <= 1; os += 2) {
                    if (sd == 0 && od == 0) continue;
                    uint32_t oct = (octant + 8u + (uint32_t)(os * od)) & 7u;
                    uint32_t entry = rd32(table + 4u * (sec * R4_PVS_COLUMNS + oct));
                    if ((entry & 0xFFE00003u) != 0x80000000u) continue;
                    uint32_t n = rd32(entry);
                    if (n > R4_PVS_LIST_MAX) n = R4_PVS_LIST_MAX;
                    uint32_t add[R4_PVS_LIST_MAX];
                    for (uint32_t i = 0; i < n; i++)
                        add[i] = blocks + R4_PVS_BLOCK_STRIDE * rd16(entry + 4u + 2u * i);
                    (void)r4_pvs_union(list, &count, add, n, R4_PVS_LIST_MAX);
                }
            }
        }
    }
    for (uint32_t i = r.before; i < count; i++)
        wr32(R4_PVS_LIST_ADDR + 4u + 4u * i, list[i]);
    if (count != r.before) wr32(R4_PVS_LIST_ADDR, count);
    r.after = count;
    return r;
}

#endif /* R4_PVS_H */
