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

#include <math.h>
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

/* The course centreline: *R4_TRACK_SEGMENT_TABLE_ADDR points at one record
 * per track segment (0x3C bytes; the segment lookup 0x800269DC indexes it
 * with seg * 60), holding the segment's x and z (world >> 8, s32) at +8 and
 * +0xC. Racing runs toward lower segment numbers. */
#define R4_TRACK_SEGMENT_TABLE_ADDR 0x800AC05Cu
#define R4_TRACK_SEGMENT_STRIDE     0x3Cu
#define R4_TRACK_SEGMENT_X          0x08u
#define R4_TRACK_SEGMENT_Z          0x0Cu

/* Which way along the track the camera looks: -1 when it faces the racing
 * direction (toward lower sections), +1 when it faces the other way, from
 * the camera yaw and the centreline over the five segments of `section`
 * (the next section toward lower numbers). 0 when the centreline is not
 * readable. */
static inline int r4_pvs_ahead_dir(R4PvsRead32 rd32, uint32_t section, uint32_t nsec,
                                   uint32_t yaw)
{
    uint32_t segs = rd32(R4_TRACK_SEGMENT_COUNT_ADDR);
    uint32_t tab = rd32(R4_TRACK_SEGMENT_TABLE_ADDR);
    if (nsec == 0u || segs == 0u || section >= nsec || (tab & 0xFFE00003u) != 0x80000000u)
        return 0;
    uint32_t s0 = (5u * section) % segs, s1 = (s0 + segs - 5u % segs) % segs;
    uint32_t r0 = tab + R4_TRACK_SEGMENT_STRIDE * s0, r1 = tab + R4_TRACK_SEGMENT_STRIDE * s1;
    int32_t x0 = (int32_t)rd32(r0 + R4_TRACK_SEGMENT_X), z0 = (int32_t)rd32(r0 + R4_TRACK_SEGMENT_Z);
    int32_t x1 = (int32_t)rd32(r1 + R4_TRACK_SEGMENT_X), z1 = (int32_t)rd32(r1 + R4_TRACK_SEGMENT_Z);
    if (x0 == x1 && z0 == z1) return 0;
    /* The camera looks along (sin, cos) of its yaw (yaw = atan2(dx, dz),
     * 4096 per turn); it faces the racing direction when that has a
     * positive component along the centreline toward lower segments. */
    double t = (double)(yaw & 0xFFFu) * (6.283185307179586 / 4096.0);
    double dot = sin(t) * ((double)x1 - x0) + cos(t) * ((double)z1 - z0);
    return dot >= 0.0 ? -1 : 1;
}

typedef struct {
    int ok;                   /* the guest's table and list looked valid */
    uint32_t before, after;   /* list length on entry and on exit */
} R4PvsMerge;

/* Append to the frame list the blocks of the sections around the camera's
 * (`ahead` of them in direction `dir`, -1 or +1, and `behind` the other
 * way), each with octants +/- od for od <= oct_reach, except the camera's
 * own entry, which the list already holds. Nearest first: the camera's
 * section, then one ahead, one behind, two ahead, two behind ..., and
 * within each the octants in the order 0, -1, +1, -2, +2, so the 255-entry
 * cap drops the farthest additions. With no section reach this is exactly
 * the widescreen octant union. Sections wrap around the circuit; a section
 * reach needs a plausible `nsec` (r4_pvs_section_count) and is dropped
 * without one, and no section is visited twice on a short circuit. With
 * `keep`, a block from another entry is appended only when keep(ctx, block)
 * is nonzero (the camera's own entry is the game's list and is never
 * filtered).
 * Nothing is written when the guest's table or list does not look valid. */
typedef int (*R4PvsKeep)(void *ctx, uint32_t block);

static inline R4PvsMerge r4_pvs_merge_dir(R4PvsRead32 rd32, R4PvsRead16 rd16,
                                          R4PvsWrite32 wr32, uint32_t section,
                                          uint32_t octant, uint32_t nsec, int dir,
                                          int ahead, int behind, int oct_reach,
                                          R4PvsKeep keep, void *keep_ctx)
{
    R4PvsMerge r = { 0, 0u, 0u };
    if (ahead < 0) ahead = 0;
    if (behind < 0) behind = 0;
    if (oct_reach < 0) oct_reach = 0;
    if (oct_reach > 4) oct_reach = 4;
    if (dir >= 0) dir = 1; else dir = -1;
    if (nsec == 0u || section >= nsec) ahead = behind = 0;
    if (nsec != 0u) {
        /* At most nsec - 1 other sections: drop the farthest first. */
        while (ahead + behind > (int)nsec - 1) {
            if (behind >= ahead && behind > 0) behind--;
            else ahead--;
        }
    }
    uint32_t table = rd32(R4_PVS_TABLE_PTR_ADDR);
    uint32_t blocks = rd32(R4_PVS_BLOCK_BASE_ADDR);
    if ((table & 0xFFE00003u) != 0x80000000u || (blocks & 0xFFE00000u) != 0x80000000u)
        return r;
    uint32_t list[R4_PVS_LIST_MAX], count = rd32(R4_PVS_LIST_ADDR);
    if (count > R4_PVS_LIST_MAX) return r;
    for (uint32_t i = 0; i < count; i++) list[i] = rd32(R4_PVS_LIST_ADDR + 4u + 4u * i);
    r.ok = 1;
    r.before = r.after = count;
    if (ahead == 0 && behind == 0 && oct_reach == 0) return r;
    int far = ahead > behind ? ahead : behind;
    for (int sd = 0; sd <= far; sd++) {
        for (int side = 0; side < 2; side++) {
            if (sd == 0 && side) continue;
            if (sd > 0 && sd > (side ? behind : ahead)) continue;
            int delta = sd == 0 ? 0 : (side ? -dir : dir) * sd;
            uint32_t sec = delta ? r4_pvs_section_step(section, delta, nsec) : section;
            for (int od = 0; od <= oct_reach; od++) {
                for (int os = od ? -1 : 1; os <= 1; os += 2) {
                    if (sd == 0 && od == 0) continue;
                    uint32_t oct = (octant + 8u + (uint32_t)(os * od)) & 7u;
                    uint32_t entry = rd32(table + 4u * (sec * R4_PVS_COLUMNS + oct));
                    if ((entry & 0xFFE00003u) != 0x80000000u) continue;
                    uint32_t n = rd32(entry);
                    if (n > R4_PVS_LIST_MAX) n = R4_PVS_LIST_MAX;
                    uint32_t add[R4_PVS_LIST_MAX], nadd = 0;
                    for (uint32_t i = 0; i < n; i++) {
                        uint32_t b = blocks + R4_PVS_BLOCK_STRIDE * rd16(entry + 4u + 2u * i);
                        if (!keep || keep(keep_ctx, b)) add[nadd++] = b;
                    }
                    (void)r4_pvs_union(list, &count, add, nadd, R4_PVS_LIST_MAX);
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

/* The symmetric form: `sec_reach` sections each way (toward lower sections
 * first). */
static inline R4PvsMerge r4_pvs_merge(R4PvsRead32 rd32, R4PvsRead16 rd16,
                                      R4PvsWrite32 wr32, uint32_t section,
                                      uint32_t octant, uint32_t nsec,
                                      int sec_reach, int oct_reach)
{
    if (sec_reach < 0) sec_reach = 0;
    if (nsec != 0u && 2 * sec_reach + 1 > (int)nsec) sec_reach = ((int)nsec - 1) / 2;
    return r4_pvs_merge_dir(rd32, rd16, wr32, section, octant, nsec, -1,
                            sec_reach, sec_reach, oct_reach, 0, 0);
}

#endif /* R4_PVS_H */
