/* R4's link setup with three or four synchronized netplay seats.
 *
 * The serial initializers establish guest state and SIO1 events. In a
 * three- or four-seat recomp-net match the events are unused, but the guest RAM
 * initialization remains required. Keep these replacements exclusive to the
 * authenticated US link overlay and the opted-in link session. The packet
 * receive path consumes synchronized seat commands instead of SIO1 bytes. */
#include "mod_plugins.h"
#include "psx_netplay.h"
#include "cpu_state.h"
#include "r4_link_analog_steering.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define R4_LINK_MODE 0x800F4EF4u
#define R4_LINK_OVERLAY_BASE 0x801149A8u

/* Expansion-1 mod memory is included in savestates and rollback snapshots. */
static uint32_t r4_link_brake_ramps;
/* Two camera position/angle pairs beyond the retail pair. Kept in guest
 * memory so replay and rollback restore each view's previous camera state. */
static uint32_t r4_link_extra_view_cameras;
static uint32_t r4_link_extra_ot;
/* Unused bytes after the overlay's receive/consume ring indices (BC/BD).
 * These guest bytes are restored by rollback/load without changing the
 * existing enhancement-memory allocation layout. */
#define R4_LINK_PAUSE_LATCH 0x801190BEu
#define R4_LINK_MENU_DIRECTION_LATCH 0x801190BFu
#define R4_LINK_OT_BYTES 0xB00u
#define R4_LINK_BUFFER_BYTES 0x22778u
#define R4_LINK_OT_ALLOC_BYTES (4u * R4_LINK_OT_BYTES + 8u)
static unsigned r4_link_draw_view;
static uint32_t r4_link_hud_start[4], r4_link_hud_end[4];
static unsigned r4_link_hud_open = 4;
static uint32_t r4_link_hud_copy_head, r4_link_hud_copy_tail;
static int r4_link_patch_first_clip(uint32_t root, unsigned view, unsigned seats);
static uint32_t r4_link_buffer_index(void);

/* The extra-seat speed and gear calls reuse a valid two-seat style record.
 * Their sprites are linked into the retail OT. The extra views get bounded
 * copies after their roads; remove the source links before root-1 DMA so
 * those values cannot cover the first two racers' HUDs. */
static void r4_link_unlink_extra_hud(struct CPUState *cpu, unsigned seats)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t prev = 0;
    uint32_t current = cpu->gpr[4];
    if (seats != 3u && seats != 4u) return;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        if ((current & 3u) || current < buffer ||
            current + 20u > buffer + R4_LINK_BUFFER_BYTES) break;
        const uint32_t tag = psx_mod_read_word(current);
        const unsigned words = tag >> 24;
        const uint32_t next = tag & 0x00FFFFFFu;
        int extra_sprite = words == 4u &&
            (psx_mod_read_word(current + 4u) >> 24) == 0x65u;
        if (extra_sprite) {
            extra_sprite = 0;
            for (unsigned slot = 2u; slot < seats; ++slot)
                if (current >= r4_link_hud_start[slot] &&
                    current < r4_link_hud_end[slot]) extra_sprite = 1;
        }
        if (extra_sprite) {
            if (prev) {
                const uint32_t prev_tag = psx_mod_read_word(prev);
                psx_mod_write_word(prev,
                    (prev_tag & 0xFF000000u) | next);
            } else cpu->gpr[4] = 0x80000000u | next;
        } else prev = current;
        if (next == 0x00FFFFFFu || next >= 0x00200000u) break;
        current = 0x80000000u | next;
    }
}

/* The two retail rank groups occupy the final 0xF0 bytes of each primitive
 * buffer. Each group is six POLY_FT4-sized 0x65 packets at a 0x78 stride.
 * Extra seats cannot use the game's fixed slot writes: slot 3 crosses the
 * buffer boundary and overwrites race state. Build their groups in the
 * bounded primitive heap after the game's own rank updates have completed. */
static void r4_link_place_rank_packets(unsigned seats)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t heap = psx_mod_read_word(0x1F800000u);
    const uint32_t base = buffer + 0x22688u;
    if ((seats != 3u && seats != 4u) || r4_link_buffer_index() > 1u ||
        (heap & 3u) || heap < buffer ||
        heap + 6u * (seats - 1u) * 20u >
            buffer + R4_LINK_BUFFER_BYTES) return;
    for (unsigned seat = 0; seat < seats; ++seat) {
        const unsigned source_seat = seat & 1u;
        const int clone = seat >= 1u;
        const int dx = seats == 4u ? (seat & 1u ? 160 : 0) :
                       (seat == 2u ? 160 : 0);
        const int dy = seats == 4u ? (seat == 1u ? -120 :
                                      seat == 2u ? 120 : 0) :
                       (seat == 2u ? 120 : 0);
        for (unsigned packet = 0; packet < 6u; ++packet) {
            const uint32_t source = base + 0x78u * source_seat + 20u * packet;
            const uint32_t tag = psx_mod_read_word(source);
            if ((tag >> 24) != 4u ||
                (psx_mod_read_word(source + 4u) >> 24) != 0x65u) return;
            const uint32_t dest = clone ? heap : source;
            if (clone) {
                for (unsigned word = 0; word < 5u; ++word)
                    psx_mod_write_word(dest + 4u * word,
                                       psx_mod_read_word(source + 4u * word));
                psx_mod_write_word(dest, 0x04FFFFFFu);
            }
            if (dx || dy) {
                const uint32_t xy = psx_mod_read_word(dest + 8u);
                const int x = (int16_t)xy + dx;
                const int y = (int16_t)(xy >> 16) + dy;
                psx_mod_write_word(dest + 8u,
                    ((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
            }
            if (clone) {
                if (packet == 2u && seat >= 2u) {
                    const uint32_t car = psx_mod_read_word(0x800FFDD0u + 4u * seat);
                    const unsigned rank = psx_mod_read_half(car + 0x1EEu);
                    if (rank < 4u) {
                        const uint32_t uv = psx_mod_read_word(dest + 12u);
                        psx_mod_write_word(dest + 12u,
                            (uv & 0xFFFFFF00u) | ((rank + 1u) * 24u));
                    }
                }
                if (r4_link_hud_copy_tail) {
                    const uint32_t tail = psx_mod_read_word(r4_link_hud_copy_tail);
                    psx_mod_write_word(r4_link_hud_copy_tail,
                        (tail & 0xFF000000u) | (dest & 0x00FFFFFFu));
                } else r4_link_hud_copy_head = dest;
                r4_link_hud_copy_tail = dest;
                heap += 20u;
            }
        }
    }
    psx_mod_write_word(0x1F800000u, heap);
}

static void r4_link_hud_boundary(unsigned next)
{
    const uint32_t heap = psx_mod_read_word(0x1F800000u);
    if (r4_link_hud_open < 4u)
        r4_link_hud_end[r4_link_hud_open] = heap;
    r4_link_hud_open = next;
    if (next < 4u) r4_link_hud_start[next] = heap;
}

static uint32_t r4_link_buffer_index(void)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    if (buffer == 0x800ADCA0u) return 0;
    if (buffer == 0x800ADCA0u + R4_LINK_BUFFER_BYTES) return 1;
    return UINT32_MAX;
}

static uint32_t r4_link_extra_ot_for(unsigned view)
{
    const uint32_t buffer = r4_link_buffer_index();
    if (!r4_link_extra_ot || buffer > 1 || view < 2 || view > 3) return 0;
    return r4_link_extra_ot + R4_LINK_OT_BYTES * (2u * buffer + view - 2u);
}

static void r4_link_clear_extra_ots(void)
{
    static unsigned clear_trace_count;
    for (unsigned view = 2; view < 4; ++view) {
        const uint32_t base = r4_link_extra_ot_for(view);
        if (!base) {
            if (getenv("PSX_R4_VIEW_TRACE") && clear_trace_count++ < 4)
                fprintf(stderr, "r4-view-ot-clear-miss view=%u buf=%08X alloc=%08X\n",
                        view, psx_mod_read_word(0x800ACDCCu), r4_link_extra_ot);
            return;
        }
        for (uint32_t i = 0; i < R4_LINK_OT_BYTES / 4u; ++i)
            psx_mod_write_word(base + 4u * i,
                               i ? ((base + 4u * (i - 1u)) & 0x00FFFFFFu)
                                 : 0x00FFFFFFu);
        if (getenv("PSX_R4_VIEW_TRACE") && clear_trace_count++ < 4)
            fprintf(stderr, "r4-view-ot-clear view=%u base=%08X w0=%08X w1=%08X\n",
                    view, base, psx_mod_read_word(base), psx_mod_read_word(base + 4u));
    }
    psx_mod_write_word(r4_link_extra_ot + 4u * R4_LINK_OT_BYTES +
                       4u * r4_link_buffer_index(),
                       psx_mod_read_word(0x800AC064u));
}

/* The viewport helper prepends one draw-environment packet to the retail OT
 * selected by viewport parity. Move it out before the next view (or before
 * DrawOTag for the final view), so its clip stays paired with that view's
 * primitive table. The extra OT's word zero holds this packet until submit. */
static int r4_link_extract_view_env(unsigned view)
{
    const uint32_t extra = r4_link_extra_ot_for(view);
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    if (!extra || view < 2u || view > 3u) return 0;
    const uint32_t slot = buffer + (view == 2u ? 0xB6Cu : 0x166Cu);
    const uint32_t head = psx_mod_read_word(slot);
    const uint32_t packet = head & 0x00FFFFFFu;
    const uint32_t packet_guest = 0x80000000u | packet;
    if ((packet & 3u) || packet_guest < buffer + 0x1670u ||
        packet_guest >= buffer + R4_LINK_BUFFER_BYTES ||
        psx_mod_read_word(extra) != 0x00FFFFFFu) return 0;
    const uint32_t tag = psx_mod_read_word(packet_guest);
    const uint32_t old = tag & 0x00FFFFFFu;
    if ((old & 3u) || old == packet ||
        ((0x80000000u | old) < buffer ||
         (0x80000000u | old) >= buffer + R4_LINK_BUFFER_BYTES)) return 0;
    psx_mod_write_word(slot, (head & 0xFF000000u) | old);
    psx_mod_write_word(extra, packet);
    return 1;
}

static int r4_link_append_ot(uint32_t root, unsigned seats)
{
    const uint32_t extra2 = r4_link_extra_ot_for(2);
    const uint32_t extra3 = r4_link_extra_ot_for(3);
    if (!extra2 || (seats == 4 && !extra3)) return 0;
    uint32_t current = root;
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    const uint32_t buffer_index = r4_link_buffer_index();
    if (buffer_index > 1 ||
        psx_mod_read_word(r4_link_extra_ot + 4u * R4_LINK_OT_BYTES +
                          4u * buffer_index) != psx_mod_read_word(0x800AC064u) ||
        psx_mod_read_word(extra2 + 4u) != (extra2 & 0x00FFFFFFu)) return 0;
    const uint32_t env2 = psx_mod_read_word(extra2) & 0x00FFFFFFu;
    const uint32_t env3 = seats == 4 ?
        (psx_mod_read_word(extra3) & 0x00FFFFFFu) : 0;
    if (env2 == 0x00FFFFFFu || (seats == 4 && env3 == 0x00FFFFFFu) ||
        (env2 & 3u) || (env3 & 3u) ||
        (0x80000000u | env2) < buffer + 0x1670u ||
        (0x80000000u | env2) >= buffer + R4_LINK_BUFFER_BYTES ||
        (seats == 4 && ((0x80000000u | env3) < buffer + 0x1670u ||
                        (0x80000000u | env3) >= buffer + R4_LINK_BUFFER_BYTES))) return 0;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        if ((current & 3u) || current < buffer ||
            current >= buffer + R4_LINK_BUFFER_BYTES) return 0;
        const uint32_t tag = psx_mod_read_word(current);
        const uint32_t next = tag & 0x00FFFFFFu;
        const uint32_t next_guest = 0x80000000u | next;
        if (next == 0x00FFFFFFu || next_guest < buffer ||
            next_guest >= buffer + R4_LINK_BUFFER_BYTES) {
            if (next != 0x00FFFFFFu &&
                ((next & 3u) || next >= 0x00200000u)) return 0;
            /* Keep the game's shared packet/terminator after both extra OTs;
             * only the predecessor in this frame's private chain changes. */
            const uint32_t env2_tag = psx_mod_read_word(0x80000000u | env2);
            psx_mod_write_word(0x80000000u | env2,
                (env2_tag & 0xFF000000u) |
                ((extra2 + R4_LINK_OT_BYTES - 4u) & 0x00FFFFFFu));
            psx_mod_write_word(extra2, seats == 4 ? env3 : next);
            if (seats == 4) {
                const uint32_t env3_tag = psx_mod_read_word(0x80000000u | env3);
                psx_mod_write_word(0x80000000u | env3,
                    (env3_tag & 0xFF000000u) |
                    ((extra3 + R4_LINK_OT_BYTES - 4u) & 0x00FFFFFFu));
                psx_mod_write_word(extra3, next);
            }
            psx_mod_write_word(current, (tag & 0xFF000000u) |
                                      env2);
            (void)r4_link_patch_first_clip(0x80000000u | env2, 2, seats);
            if (seats == 4)
                (void)r4_link_patch_first_clip(0x80000000u | env3, 3, seats);
            return 1;
        }
        current = next_guest;
    }
    return 0;
}

typedef struct {
    uint16_t center_x, center_y, left, top, right, bottom;
    uint16_t rect_x, rect_y, rect_w, rect_h;
} R4LinkViewport;

/* EXE 0x8006F2B0 reads ten fields from these authenticated per-viewport
 * tables. Split Link Battle passes call it with indices 2..5. Patch just the
 * selected record during that native call, then restore the game's values. */
static const uint32_t r4_link_viewport_fields[10] = {
    0x800A3D5Cu, 0x800A3D6Cu, 0x800A3D98u, 0x800A3DA8u,
    0x800A3DB8u, 0x800A3DC8u, 0x800A3DD8u, 0x800A3DDAu,
    0x800A3DDCu, 0x800A3DDEu,
};
static R4LinkViewport r4_link_saved_viewport;
static uint32_t r4_link_saved_projection;
static unsigned r4_link_patched_viewport;

static void r4_link_viewport_record(unsigned index, R4LinkViewport *out)
{
    uint16_t *fields = (uint16_t *)out;
    for (unsigned i = 0; i < 10; ++i)
        fields[i] = psx_mod_read_half(r4_link_viewport_fields[i] +
                                      (i < 6 ? 2u * index : 8u * index));
}

static void r4_link_write_viewport_record(unsigned index,
                                           const R4LinkViewport *record)
{
    const uint16_t *fields = (const uint16_t *)record;
    for (unsigned i = 0; i < 10; ++i)
        psx_mod_write_half(r4_link_viewport_fields[i] +
                           (i < 6 ? 2u * index : 8u * index), fields[i]);
}

static void r4_link_layout_viewport(unsigned seats, unsigned view,
                                    R4LinkViewport *record)
{
    unsigned x, y, width, height;
    if (seats == 3 && view == 0) {
        x = 0; y = 0; width = 320; height = 120;
    } else if (seats == 3) {
        x = view == 1 ? 0 : 160;
        y = 120; width = 160; height = 120;
    } else {
        x = (view & 1u) ? 160 : 0;
        y = view >= 2 ? 120 : 0;
        width = 160; height = 120;
    }
    record->center_x = (uint16_t)(x + width / 2);
    record->center_y = (uint16_t)(y + height / 2);
    record->left = (uint16_t)x;
    record->top = (uint16_t)y;
    record->right = (uint16_t)(x + width);
    record->bottom = (uint16_t)(y + height);
    record->rect_x = (uint16_t)x;
    record->rect_y = (uint16_t)y;
    record->rect_w = (uint16_t)width;
    record->rect_h = (uint16_t)height;
}

/* Draw the extra racers' HUD after their roads. Their original sprites are
 * already linked into the retail OT before the private road OTs, so the road
 * covers them. Clone only linked HUD texture-state and sprite packets into
 * this frame's unused primitive heap, preserving the game's packet order.
 * The fixed rank groups are handled separately by r4_link_place_rank_packets. */
static void r4_link_copy_hud_packets(uint32_t root, unsigned seats)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t current = root;
    if (seats != 3u && seats != 4u) return;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        if ((current & 3u) || current < buffer ||
            current + 8u > buffer + R4_LINK_BUFFER_BYTES) return;
        const uint32_t tag = psx_mod_read_word(current);
        const unsigned words = tag >> 24;
        const uint32_t next = tag & 0x00FFFFFFu;
        if ((words == 2u || words == 4u) &&
            current + 4u * (words + 1u) <= buffer + R4_LINK_BUFFER_BYTES) {
            const uint32_t op = psx_mod_read_word(current + 4u) >> 24;
            if ((words == 2u && op == 0xE1u &&
                 psx_mod_read_word(current + 8u) == 0u) ||
                (words == 4u && op == 0x65u)) {
                for (unsigned slot = 2; slot < seats; ++slot) {
                    const uint32_t start = r4_link_hud_start[slot];
                    const uint32_t end = r4_link_hud_end[slot];
                    if (end <= start || end - start > 0x1000u ||
                        current < start || current >= end) continue;
                    const uint32_t heap = psx_mod_read_word(0x1F800000u);
                    const uint32_t bytes = 4u * (words + 1u);
                    if ((heap & 3u) || heap < buffer ||
                        heap + bytes > buffer + R4_LINK_BUFFER_BYTES) return;
                    for (unsigned i = 0; i <= words; ++i)
                        psx_mod_write_word(heap + 4u * i,
                                           psx_mod_read_word(current + 4u * i));
                    psx_mod_write_word(heap,
                                       (tag & 0xFF000000u) | 0x00FFFFFFu);
                    if (op == 0x65u) {
                        const uint32_t xy = psx_mod_read_word(heap + 8u);
                        int x = (int16_t)xy;
                        int y = (int16_t)(xy >> 16);
                        const int right = seats == 3u || slot == 3u;
                        if (right && x < 96) x += 160;
                        if (!right && x >= 225) x -= 160;
                        /* The game's extra-slot HUD positions may advance
                         * by two or three 120-line view strides. Fold both
                         * into the lower view after the private road OTs. */
                        if (y >= 240) y = 120 + y % 120;
                        else if (y >= 0 && y < 120) y += 120;
                        psx_mod_write_word(heap + 8u,
                            ((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
                    }
                    if (r4_link_hud_copy_tail) {
                        const uint32_t tail_tag =
                            psx_mod_read_word(r4_link_hud_copy_tail);
                        psx_mod_write_word(r4_link_hud_copy_tail,
                            (tail_tag & 0xFF000000u) |
                            (heap & 0x00FFFFFFu));
                    } else r4_link_hud_copy_head = heap;
                    r4_link_hud_copy_tail = heap;
                    psx_mod_write_word(0x1F800000u, heap + bytes);
                    break;
                }
            }
        }
        if (next == 0x00FFFFFFu || next >= 0x00200000u) return;
        current = 0x80000000u | next;
    }
}

/* Retail view HUD packets remain in the original OT. Move only their linked
 * textured sprites into the selected compact viewport before DMA reads it. */
static void r4_link_place_retail_hud(uint32_t root, unsigned seats)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t current = root;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        if ((current & 3u) || current < buffer ||
            current + 20u > buffer + R4_LINK_BUFFER_BYTES) return;
        const uint32_t tag = psx_mod_read_word(current);
        if ((tag >> 24) == 4u &&
            (psx_mod_read_word(current + 4u) >> 24) == 0x65u) {
            for (unsigned slot = 0; slot < 2u; ++slot) {
                const uint32_t start = r4_link_hud_start[slot];
                const uint32_t end = r4_link_hud_end[slot];
                if (end <= start || end - start > 0x1000u ||
                    current < start || current >= end) continue;
                const uint32_t xy = psx_mod_read_word(current + 8u);
                int x = (int16_t)xy;
                int y = (int16_t)(xy >> 16);
                if (seats == 3u && slot == 1u) {
                    if (x >= 225) x -= 160;
                    if (y >= 240) y -= 120;
                    else if (y >= 0 && y < 120) y += 120;
                } else if (seats == 4u) {
                    if (slot == 0u && x >= 225) x -= 160;
                    if (slot == 1u && x < 96) x += 160;
                    if (y >= 120 && y < 240) y -= 120;
                }
                psx_mod_write_word(current + 8u,
                    ((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
                break;
            }
        }
        const uint32_t next = tag & 0x00FFFFFFu;
        if (next == 0x00FFFFFFu || next >= 0x00200000u) return;
        current = 0x80000000u | next;
    }
}

static void r4_link_append_hud_overlay(unsigned seats)
{
    if (!r4_link_hud_copy_head || !r4_link_hud_copy_tail) return;
    const uint32_t extra = r4_link_extra_ot_for(seats - 1u);
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    const uint32_t heap = psx_mod_read_word(0x1F800000u);
    if (!extra || (heap & 3u) || heap < buffer ||
        heap + 24u > buffer + R4_LINK_BUFFER_BYTES) return;
    const uint32_t old = psx_mod_read_word(extra) & 0x00FFFFFFu;
    const uint32_t y = r4_link_buffer_index() ? 240u : 0u;
    psx_mod_write_word(heap, 0x03000000u |
                             (r4_link_hud_copy_head & 0x00FFFFFFu));
    psx_mod_write_word(heap + 4u, 0xE3000000u | (y << 10));
    psx_mod_write_word(heap + 8u, 0xE4000000u | ((y + 239u) << 10) | 319u);
    psx_mod_write_word(heap + 12u, 0xE5000000u | (y << 11));
    const uint32_t tail_tag = psx_mod_read_word(r4_link_hud_copy_tail);
    psx_mod_write_word(r4_link_hud_copy_tail,
                       (tail_tag & 0xFF000000u) |
                       ((heap + 16u) & 0x00FFFFFFu));
    /* The next frame's road pass inherits the final GPU draw mode. */
    psx_mod_write_word(heap + 16u, 0x01000000u | old);
    psx_mod_write_word(heap + 20u, 0xE1000205u);
    psx_mod_write_word(extra, heap & 0x00FFFFFFu);
    psx_mod_write_word(0x1F800000u, heap + 24u);
}

/* Some prebuilt draw-environment packets use the retail two-view clipping
 * rectangle on the second frame buffer. Rewrite the first E3/E4 pair in each
 * private view chain after all primitives have been linked, leaving shared
 * full-screen HUD packets and unrelated draw state alone. */
static int r4_link_patch_first_clip(uint32_t root, unsigned view, unsigned seats)
{
    if (view >= seats || (seats != 3 && seats != 4)) return 0;
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    const uint32_t y_offset = r4_link_buffer_index() ? 240u : 0u;
    R4LinkViewport rect;
    r4_link_layout_viewport(seats, view, &rect);
    uint32_t current = root;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        const int in_buffer = current >= buffer &&
                              current + 4u <= buffer + R4_LINK_BUFFER_BYTES;
        const int in_extra = current >= r4_link_extra_ot &&
                             current + 4u <= r4_link_extra_ot + R4_LINK_OT_ALLOC_BYTES;
        if ((current & 3u) || (!in_buffer && !in_extra)) return 0;
        const uint32_t tag = psx_mod_read_word(current);
        const unsigned words = tag >> 24;
        if (in_buffer && words >= 2u && words <= 0xFFu &&
            current + 4u * (words + 1u) <= buffer + R4_LINK_BUFFER_BYTES) {
            for (unsigned i = 0; i + 1u < words; ++i) {
                const uint32_t a = current + 4u * (i + 1u);
                if ((psx_mod_read_word(a) >> 24) != 0xE3u ||
                    (psx_mod_read_word(a + 4u) >> 24) != 0xE4u) continue;
                psx_mod_write_word(a, 0xE3000000u |
                    (((uint32_t)rect.top + y_offset) << 10) | rect.left);
                psx_mod_write_word(a + 4u, 0xE4000000u |
                    (((uint32_t)rect.bottom - 1u + y_offset) << 10) |
                    (rect.right - 1u));
                return 1;
            }
        }
        const uint32_t next = tag & 0x00FFFFFFu;
        if (next == 0x00FFFFFFu) return 0;
        current = 0x80000000u | next;
    }
    return 0;
}

static void r4_link_prepare_viewport(unsigned seats, unsigned view)
{
    const unsigned index = view + 2u;
    R4LinkViewport rect;
    if ((seats != 3 && seats != 4) || view >= seats) return;
    if (r4_link_patched_viewport) {
        r4_link_write_viewport_record(r4_link_patched_viewport,
                                       &r4_link_saved_viewport);
        psx_mod_write_word(0x800A3D7Cu + 4u * r4_link_patched_viewport,
                           r4_link_saved_projection);
    }
    r4_link_viewport_record(index, &r4_link_saved_viewport);
    r4_link_saved_projection = psx_mod_read_word(0x800A3D7Cu + 4u * index);
    r4_link_layout_viewport(seats, view, &rect);
    r4_link_write_viewport_record(index, &rect);
    /* Preserve the full-screen horizontal field of view at half width. */
    psx_mod_write_word(0x800A3D7Cu + 4u * index,
                       rect.rect_w == 160 ? 145u : 290u);
    r4_link_patched_viewport = index;
}

static void r4_link_patch_view_loop(unsigned seats)
{
    /* The HUD loop at 0x80115F58..6C increments s0 by 0x320. Only the
     * first two car structs are adjacent. Its 0x15F60 delay-slot increment
     * of s1 runs before this trampoline, which loads roster[s1] into s0.
     * The authenticated overlay's 0x801190C0..D8 tail is all zero padding,
     * beyond its packet/ring state ending at 0x801190BD. */
    static const uint32_t trampoline[] = {
        0x3C088010u, /* lui t0,0x8010 */
        0x00114880u, /* sll t1,s1,2 */
        0x01094021u, /* addu t0,t0,t1 */
        0x8D10FDD0u, /* lw s0,-0x230(t0): roster[s1] */
        0x08000000u | ((0x80115F64u >> 2) & 0x03FFFFFFu),
        0x00000000u,
    };
    if (psx_mod_read_word(0x80115D90u) != 0x24140002u ||
        psx_mod_read_word(0x80115F5Cu) != 0x26100320u) return;
    for (unsigned i = 0; i < 6; ++i)
        if (psx_mod_read_word(0x801190C0u + 4u * i) != 0) return;
    for (unsigned i = 0; i < 6; ++i)
        psx_mod_write_code_word(0x801190C0u + 4u * i, trampoline[i]);
    psx_mod_write_code_word(0x80115F5Cu,
                            0x08000000u | ((0x801190C0u >> 2) & 0x03FFFFFFu));
    psx_mod_write_code_word(0x80115D90u, 0x24140000u | seats);
}

static int r4_link_session(void)
{
    const char *probe = getenv("PSX_GAME_FILTER_OFFLINE_PROBE");
    const char *experimental = getenv("PSX_R4_LINK_EXPERIMENTAL");
    const int seats = psx_netplay_seat_count();
    return ((seats == 3 || seats == 4) && experimental &&
            experimental[0] == '1' && !experimental[1]) ||
           (probe && probe[0] == '1' && !probe[1]);
}

static int r4_link_ready(void)
{
    return r4_link_session() &&
           psx_mod_read_half(R4_LINK_MODE) == 4 &&
           psx_mod_read_word(R4_LINK_OVERLAY_BASE) == 9u &&
           psx_mod_read_word(0x80114C38u) == 0x0C024BA5u &&
           psx_mod_read_word(0x801155CCu) == 0x0C024AECu;
}

static void r4_link_zero_words(uint32_t address, unsigned words)
{
    for (unsigned i = 0; i < words; ++i)
        psx_mod_write_word(address + 4u * i, 0);
}

static int r4_link_probe_commands(uint32_t words[4])
{
    const char *cursor = getenv("PSX_R4_LINK_PROBE_COMMANDS");
    if (!cursor || !*cursor) return 0;
    for (unsigned i = 0; i < 4; ++i) {
        char *end;
        unsigned long word = strtoul(cursor, &end, 16);
        if (end == cursor || word > UINT32_MAX ||
            (i < 3 ? *end != ',' : *end != '\0')) return 0;
        words[i] = (uint32_t)word;
        cursor = end + (i < 3);
    }
    return 1;
}

static void r4_link_stage_commands(const uint32_t words[4])
{
    const int side = psx_mod_read_half(0x800AD6C0u) != 0;
    const uint32_t first = side ? 0x800ACDA8u : 0x800ACD98u;
    const uint32_t second = side ? 0x800ACD98u : 0x800ACDA8u;
    for (unsigned i = 0; i < 2; ++i) {
        psx_mod_write_word(first + 4u * i, words[i]);
        psx_mod_write_word(second + 4u * i, words[2 + i]);
    }
}

/* R4's default digital and DualShock control-preset command encoding. */
static uint32_t r4_link_digital_command(unsigned seat, const PsxNetPad *pad)
{
    uint32_t word = pad && pad->analog ? 0x80000000u : 0x20000000u;
    if (!r4_link_brake_ramps) return word;
    uint8_t ramp = psx_mod_read_byte(r4_link_brake_ramps + seat);
    if (!pad || !pad->connected) {
        psx_mod_write_byte(r4_link_brake_ramps + seat, 0);
        return word;
    }
    if (!(pad->buttons & 0x4000u)) word |= 0x0000FF00u; /* X: throttle */
    if (!(pad->buttons & 0x8000u)) { /* Square: brake */
        ramp = ramp >= 0x6Fu ? 0x73u : (uint8_t)(ramp + 4u);
        word |= (uint32_t)(0x8Cu + ramp) << 16;
    } else {
        ramp = 0;
    }
    psx_mod_write_byte(r4_link_brake_ramps + seat, ramp);
    if (!(pad->buttons & 0x0080u)) word |= 0x01000000u; /* left */
    if (!(pad->buttons & 0x0020u)) word |= 0x02000000u; /* right */
    if (pad->analog) {
        uint8_t steering = r4_link_analog_steering[pad->lx];
        /* R4 uses D-pad steering when the stick is in its center deadzone;
         * right wins if both directions are held. */
        if (!steering) {
            if (!(pad->buttons & 0x0020u)) steering = r4_link_analog_steering[255];
            else if (!(pad->buttons & 0x0080u)) steering = r4_link_analog_steering[0];
        }
        word |= steering;
    }
    return word;
}

static int r4_link_stage_netplay_commands(void)
{
    const int seats = psx_netplay_seat_count();
    if (seats != 3 && seats != 4) return 0;
    uint32_t words[4] = {0x20000000u, 0x20000000u,
                         0x20000000u, 0x20000000u};
    uint8_t start_held = 0;
    uint8_t up_held = 0;
    uint8_t down_held = 0;
    for (int seat = 0; seat < seats; ++seat) {
        PsxNetPad pad;
        if (!psx_netplay_sim_pad(seat, &pad)) return 0;
        words[seat] = r4_link_digital_command((unsigned)seat, &pad);
        if (!pad.connected) continue;
        if (!(pad.buttons & 0x0008u))
            start_held |= (uint8_t)(1u << seat);
        if (!(pad.buttons & 0x0010u))
            up_held |= (uint8_t)(1u << seat);
        if (!(pad.buttons & 0x0040u))
            down_held |= (uint8_t)(1u << seat);
    }
    r4_link_stage_commands(words);
    const uint8_t previous_start = psx_mod_read_byte(R4_LINK_PAUSE_LATCH);
    const uint8_t previous_directions =
        psx_mod_read_byte(R4_LINK_MENU_DIRECTION_LATCH);
    const uint8_t directions = (uint8_t)(up_held | (down_held << 4));
    /* Retail sends packet byte 3 with bit 0 for Start, bit 1 for Up, and
     * bit 2 for Down. The native pause handler ORs remote/local packet flags:
     * Start confirms the selected item, while Up/Down move its cursor. Fold
     * every admitted seat's new edges into the received flag byte. */
    uint8_t menu_flags = 0;
    if (start_held & (uint8_t)~previous_start) menu_flags |= 1u;
    if (up_held & (uint8_t)~previous_directions) menu_flags |= 2u;
    if (down_held & (uint8_t)~(previous_directions >> 4)) menu_flags |= 4u;
    psx_mod_write_byte(0x8011901Du, menu_flags);
    psx_mod_write_byte(0x8011901Eu, 0);
    psx_mod_write_byte(R4_LINK_PAUSE_LATCH, start_held);
    psx_mod_write_byte(R4_LINK_MENU_DIRECTION_LATCH, directions);
    return 1;
}

static void r4_link_clear_serial_state(void)
{
    if (r4_link_brake_ramps)
        for (unsigned seat = 0; seat < 4; ++seat)
            psx_mod_write_byte(r4_link_brake_ramps + seat, 0);
    psx_mod_write_byte(R4_LINK_PAUSE_LATCH, 0);
    psx_mod_write_byte(R4_LINK_MENU_DIRECTION_LATCH, 0);
    static const uint32_t words[] = {
        0x8011902Cu, 0x80119034u, 0x80119028u, 0x80119030u,
        0x80119018u, 0x80118FF8u, 0x80119000u, 0x80119004u,
    };
    static const uint32_t bytes[] = {
        0x8011901Fu, 0x801190BDu, 0x80119038u, 0x80119039u,
        0x8011901Du, 0x8011901Eu, 0x8011901Cu,
    };
    for (unsigned i = 0; i < sizeof words / sizeof words[0]; ++i)
        psx_mod_write_word(words[i], 0);
    for (unsigned i = 0; i < sizeof bytes / sizeof bytes[0]; ++i)
        psx_mod_write_byte(bytes[i], 0);
    psx_mod_write_byte(0x801190BCu, 7);
    for (unsigned i = 0; i < 8; ++i)
        r4_link_zero_words(0x80119050u + 12u * i, 3);
    r4_link_zero_words(0x801190B0u, 3);
    r4_link_zero_words(0x80119040u, 3);
    r4_link_zero_words(0x800F3CA0u, 2);
    r4_link_zero_words(0x800ACDA8u, 2);
    r4_link_zero_words(0x800ACD98u, 2);
}

static int r4_link_serial_filter(struct CPUState *cpu, uint32_t address)
{
    const char *view_probe = getenv("PSX_R4_VIEW_COUNT_PROBE");
    if (((address == 0x80021614u && cpu->gpr[31] == 0x80115F18u) ||
         (address == 0x80021960u && cpu->gpr[31] == 0x80115F24u)) &&
        view_probe && view_probe[0] == '1' && !view_probe[1] &&
        r4_link_ready() && cpu->gpr[5] >= 2u && cpu->gpr[5] < 4u)
        cpu->gpr[5] &= 1u; /* valid HUD record, unchanged per-car value a0 */
    if (address == 0x8006F00Cu && view_probe && view_probe[0] == '1' &&
        !view_probe[1] && r4_link_ready() && r4_link_draw_view >= 2u &&
        r4_link_draw_view < 4u)
        cpu->gpr[4] &= 1u; /* course renderer has only two view indices */
    if (address == 0x80093520u && view_probe && view_probe[0] == '1' &&
        !view_probe[1] && r4_link_ready()) {
        if (cpu->gpr[31] == 0x8001E808u) {
            r4_link_draw_view = 4;
            r4_link_place_retail_hud(cpu->gpr[4],
                                     (unsigned)psx_netplay_seat_count());
            (void)r4_link_patch_first_clip(cpu->gpr[4], 0,
                                            (unsigned)psx_netplay_seat_count());
        }
        if (cpu->gpr[31] == 0x8001E808u) {
            const int seats = psx_netplay_seat_count();
            if (seats == 3 || seats == 4)
                r4_link_extract_view_env((unsigned)seats - 1u);
        }
        if (cpu->gpr[31] == 0x8001E828u) {
            r4_link_place_retail_hud(cpu->gpr[4],
                                     (unsigned)psx_netplay_seat_count());
            if (psx_netplay_seat_count() == 4 &&
                r4_link_hud_end[3] >= r4_link_hud_start[3] &&
                r4_link_hud_end[3] + 0x80u <=
                    psx_mod_read_word(0x800ACDCCu) + R4_LINK_BUFFER_BYTES)
                r4_link_hud_end[3] += 0x80u;
            r4_link_copy_hud_packets(cpu->gpr[4],
                                     (unsigned)psx_netplay_seat_count());
            r4_link_unlink_extra_hud(cpu,
                                     (unsigned)psx_netplay_seat_count());
            r4_link_place_rank_packets((unsigned)psx_netplay_seat_count());
            (void)r4_link_patch_first_clip(cpu->gpr[4], 1,
                                            (unsigned)psx_netplay_seat_count());
            const unsigned seats = (unsigned)psx_netplay_seat_count();
            if (r4_link_append_ot(cpu->gpr[4], seats))
                r4_link_append_hud_overlay(seats);
        }
    }
    if (address == 0x80116024u && r4_link_patched_viewport) {
        r4_link_write_viewport_record(r4_link_patched_viewport,
                                       &r4_link_saved_viewport);
        psx_mod_write_word(0x800A3D7Cu + 4u * r4_link_patched_viewport,
                           r4_link_saved_projection);
        r4_link_patched_viewport = 0;
        if (cpu->gpr[17] >= 2u) {
            const uint32_t extra = r4_link_extra_ot_for(cpu->gpr[17]);
            if (extra) psx_mod_write_word(0x1F800004u, extra);
        }
    }
    if (address == 0x80115770u && view_probe && view_probe[0] == '1' &&
        !view_probe[1] && r4_link_ready()) {
        const int seats = psx_netplay_seat_count();
        if (seats == 3 || seats == 4) {
            r4_link_draw_view = 0;
            for (unsigned i = 0; i < 4u; ++i)
                r4_link_hud_start[i] = r4_link_hud_end[i] = 0;
            r4_link_hud_open = 4;
            r4_link_hud_copy_head = r4_link_hud_copy_tail = 0;
            r4_link_patch_view_loop((unsigned)seats);
            r4_link_clear_extra_ots();
        }
    }
    if (address == 0x80115F84u && view_probe && view_probe[0] == '1' &&
        !view_probe[1] && r4_link_ready())
        r4_link_prepare_viewport((unsigned)psx_netplay_seat_count(), 0);
    if (view_probe && view_probe[0] == '1' && !view_probe[1] &&
        r4_link_ready()) {
        if (address == 0x80021774u && cpu->gpr[5] < 4u)
            r4_link_hud_boundary(cpu->gpr[5]);
        if (address == 0x80021134u)
            r4_link_hud_boundary(4);
    }
    if (address == 0x80115FA8u && view_probe && view_probe[0] == '1' &&
        !view_probe[1] && r4_link_ready()) {
        const unsigned view = cpu->gpr[17];
        if (view == 1u || view == 2u || view == 3u)
            r4_link_draw_view = view;
        if (view == 3u)
            r4_link_extract_view_env(2);
        const int seats = psx_netplay_seat_count();
        if (r4_link_extra_view_cameras && view >= 2u &&
            view < (unsigned)seats) {
            /* The retail draw loop assumes consecutive car structs and
             * advances its two 16-byte camera buffers into SIO1 state after
             * view 1. Use the roster and dedicated guest camera buffers. */
            cpu->gpr[19] = psx_mod_read_word(0x800FFDD0u + 4u * view);
            cpu->gpr[22] = r4_link_extra_view_cameras + 32u * (view - 2u);
            cpu->gpr[23] = cpu->gpr[22] + 16u;
        }
        r4_link_prepare_viewport((unsigned)seats, view);
    }
    static unsigned view_trace_count;
    if (getenv("PSX_R4_VIEW_TRACE") && view_trace_count < 96 &&
        (address == 0x80115D90u ||
         address == 0x80115F84u || address == 0x80115FA8u ||
         address == 0x8011601Cu || address == 0x80116024u)) {
        fprintf(stderr, "r4-view-trace pc=%08X a0=%08X s0=%08X s1=%08X s3=%08X s4=%08X s6=%08X s7=%08X\n",
                address, cpu->gpr[4], cpu->gpr[16], cpu->gpr[17], cpu->gpr[19],
                cpu->gpr[20], cpu->gpr[22], cpu->gpr[23]);
        ++view_trace_count;
    }
    static unsigned probe_logs;
    if (probe_logs < 64 && getenv("PSX_GAME_FILTER_OFFLINE_PROBE") &&
        ((address >= 0x80114AB8u &&
          psx_mod_read_word(R4_LINK_OVERLAY_BASE) == 9u) ||
         (address == 0x8009B088u && cpu->gpr[31] == 0x8002F8A0u))) {
        fprintf(stderr, "r4 link probe hook %08X mode=%u ra=%08X overlay=%08X status=%08X errors=%08X/%08X\n",
                address, psx_mod_read_half(R4_LINK_MODE), cpu->gpr[31],
                psx_mod_read_word(R4_LINK_OVERLAY_BASE),
                psx_mod_read_word(0x80119004u),
                psx_mod_read_word(0x8011902Cu),
                psx_mod_read_word(0x80119034u));
        ++probe_logs;
    }
    if (r4_link_session() && psx_mod_read_half(R4_LINK_MODE) == 0) {
        if ((address & 0x1FFFFFFFu) == 0x0003535Cu) {
            cpu->gpr[2] = 1; /* one additional, link-only menu record */
            return 1;
        }
        if ((address & 0x1FFFFFFFu) == 0x0009B088u &&
            cpu->gpr[31] == 0x8002F8A0u &&
            cpu->gpr[4] == 0 && cpu->gpr[5] == 0) {
            cpu->gpr[2] = 0x100; /* SIO1 status bits [8:7] == 2 */
            return 1;
        }
    }
    if (r4_link_session() && psx_mod_read_half(R4_LINK_MODE) == 4 &&
        (address & 0x1FFFFFFFu) == 0x0009B088u &&
        cpu->gpr[4] == 0 && cpu->gpr[5] == 0) {
        cpu->gpr[2] = 0x100; /* connected SIO1 status during pre-race FSM */
        return 1;
    }
    if ((address & 0x1FFFFFFFu) == 0x00035EA0u &&
        r4_link_session() &&
        psx_mod_read_half(R4_LINK_MODE) == 4 &&
        psx_mod_read_word(R4_LINK_OVERLAY_BASE) == 9u &&
        psx_mod_read_half(0x800FF838u) == 3) {
        /* The serial handshake normally reports accepted entrants. Leave the
         * unused fourth car inactive in a three-peer session. */
        for (unsigned slot = 0; slot < 4; ++slot)
            psx_mod_write_byte(0x800AC074u + slot,
                               psx_netplay_seat_count() == 3 && slot == 3 ? 0 : 1);
        psx_mod_write_half(0x800FF838u, 2);
        return 0;
    }
    if ((address & 0x1FFFFFFFu) == 0x00115770u &&
        psx_netplay_seat_count() == 3 && r4_link_ready() &&
        psx_mod_read_half(0x800AC754u) == 4 &&
        psx_mod_read_byte(0x800AC077u) == 0) {
        /* The original pair-local roster reserves four slots. Once R4 has
         * initialized the three accepted cars, exclude the empty fourth
         * slot from race loops and ranking counts. */
        psx_mod_write_half(0x800AC754u, 3);
    }
    if (cpu && r4_link_ready() && cpu->gpr[4] == 0u &&
        (((address & 0x1FFFFFFFu) == 0x000970A0u &&
          (cpu->gpr[31] == 0x80114BB0u ||
           cpu->gpr[31] == 0x80114BBCu)) ||
         ((address & 0x1FFFFFFFu) == 0x00097060u &&
          (cpu->gpr[31] == 0x80114BE8u ||
           cpu->gpr[31] == 0x80114BF4u)))) {
        /* Serial setup above leaves both SIO1 event handles zero. Retail
         * teardown passes them to DisableEvent/CloseEvent; OpenBIOS treats
         * handle zero as EvCB slot zero and frees the game's CD event. Keep
         * that event alive for native Result -> Car & Course Change. */
        cpu->gpr[2] = 1u;
        return 1;
    }
    if (!r4_link_ready()) return 0;
    /* The retail HUD packet slots end at the frame-buffer boundary after
     * local seat 2. These two writers index the same slots for seat 3/4 and
     * would overwrite live race state beyond 0x800F2B90. Their extra-seat
     * graphics need a separate, bounded packet allocation before enabling. */
    if (address == 0x80021A20u && cpu->gpr[31] == 0x80115F30u &&
        cpu->gpr[5] >= 2u) return 1;
    if (address == 0x80021BA4u && cpu->gpr[31] == 0x80115F48u &&
        cpu->gpr[4] >= 3u) return 1;
    if ((address & 0x1FFFFFFFu) == 0x00115520u) {
        /* Local diagnostic: the serial packet path is intentionally absent.
         * Keep the race running long enough to inspect car input/physics. */
        return 1;
    }
    if ((address & 0x1FFFFFFFu) == 0x00115198u) {
        /* Diagnostic packet arrival: keep the link handler's receive and
         * consume indices distinct for this frame. Real netplay must supply
         * synchronized seat commands and the correct sequence contract. */
        if (psx_mod_read_byte(0x801190BCu) !=
            psx_mod_read_byte(0x801190BDu)) return 1;
        uint32_t commands[4];
        if (getenv("PSX_GAME_FILTER_OFFLINE_PROBE")) {
            if (r4_link_probe_commands(commands))
                r4_link_stage_commands(commands);
        } else if (!r4_link_stage_netplay_commands()) {
            return 1; /* no admitted pad row: do not advance the packet gate */
        }
        const uint8_t consumed = psx_mod_read_byte(0x801190BDu);
        psx_mod_write_byte(0x801190BCu, (consumed + 1u) & 7u);
        return 1;
    }
    if ((address & 0x1FFFFFFFu) == 0x00115310u) {
        /* Diagnostic stand-in for the send-success buffer rotation. */
        uint32_t commands[4];
        if (!getenv("PSX_GAME_FILTER_OFFLINE_PROBE") ||
            r4_link_probe_commands(commands)) return 1;
        psx_mod_write_word(0x800ACDA8u, psx_mod_read_word(0x800F3CA0u));
        psx_mod_write_word(0x800ACDACu, psx_mod_read_word(0x800F3CA4u));
        return 1;
    }
    switch (address & 0x1FFFFFFFu) {
    case 0x00114AB8u: /* SIO1 BIOS event setup: no serial events needed */
        r4_link_zero_words(0x80119008u, 2);
        r4_link_zero_words(0x800F2C58u, 2);
        return 1;
    case 0x00114C28u: /* ring, command buffers and status init */
        r4_link_clear_serial_state();
        return 1;
    case 0x001155C4u: /* SIO1 ready handshake */
        return 1;
    default:
        return 0;
    }
}

PSX_MOD_CONSTRUCTOR(r4_link_netplay_register)
{
    const char *experimental = getenv("PSX_R4_LINK_EXPERIMENTAL");
    if (experimental && experimental[0] == '1' && !experimental[1])
        r4_link_brake_ramps = psx_mod_alloc_guest_memory(4, 4);
    if (experimental && experimental[0] == '1' && !experimental[1])
        r4_link_extra_view_cameras = psx_mod_alloc_guest_memory(64, 4);
    if (experimental && experimental[0] == '1' && !experimental[1])
        r4_link_extra_ot = psx_mod_alloc_gpu_dma_memory(R4_LINK_OT_ALLOC_BYTES, 4);
    (void)psx_game_register_netplay_function_filter(
        0x8003535Cu, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x8009B088u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80035EA0u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80114AB8u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80114C28u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x800970A0u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80097060u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x801155C4u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115520u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115198u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115310u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115770u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115D90u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115F84u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80115FA8u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80116024u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80093520u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x8006F00Cu, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021774u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021134u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021A20u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021BA4u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021614u, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x80021960u, r4_link_serial_filter);
}
