/* R4's Link Battle as the online race for two to four synchronized netplay
 * seats (docs/ONLINE_BATTLE.md).
 *
 * Every peer runs the same native mode-4 race: one car per seat, seat order =
 * session slot order (host first). The serial initializers establish guest
 * state and SIO1 events; in a recomp-net match the events are unused, but the
 * guest RAM initialization remains required. Keep these replacements
 * exclusive to the authenticated US link overlay and the opted-in link
 * session. The packet receive path consumes synchronized seat commands
 * instead of SIO1 bytes. Every peer draws every seat's view into one quadrant
 * of the same frame (guest state stays identical); each peer then presents
 * only its own seat's quadrant through psx_netplay_present_local_view. */
#include "mod_plugins.h"
#include "psx_netplay.h"
#include "cpu_state.h"
#include "r4_link_analog_steering.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define R4_LINK_MODE 0x800F4EF4u
#define R4_LINK_OVERLAY_BASE 0x801149A8u
/* Accepted entrants at mode-4 race init (0x8003821C); 2 selects the retail
 * one-view-per-console layout in the link handler (0x801157A4). */
#define R4_LINK_ENTRANTS 0x80107328u
#define R4_LINK_PAUSED 0x800F4E18u
#define R4_LINK_VIEW_W 160u
#define R4_LINK_VIEW_H 120u

/* Netplay seats this hook path serves: 2..4, one car each. */
static int r4_link_seats_ok(int seats)
{
    return seats >= 2 && seats <= 4;
}

/* Quadrant of one seat's view: seat 0 top-left, 1 top-right, 2 bottom-left,
 * 3 bottom-right, for every seat count. */
static void r4_link_view_origin(unsigned view, unsigned *x, unsigned *y)
{
    *x = (view & 1u) ? R4_LINK_VIEW_W : 0u;
    *y = view >= 2u ? R4_LINK_VIEW_H : 0u;
}

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
/* Set by this main-loop iteration's link race handler (0x80115770), cleared
 * by its final DrawOTag: the OT and HUD packet edits below only touch a
 * frame whose views the handler just built. Result, menu and loading frames
 * of mode 4 draw other packets (and would meet stale HUD copies). */
static unsigned r4_link_views_frame;
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
    if (seats != 3u && seats != 4u) return; /* extra seats only */
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
    if (!r4_link_seats_ok((int)seats) || r4_link_buffer_index() > 1u ||
        (heap & 3u) || heap < buffer ||
        heap + 6u * (seats - 1u) * 20u >
            buffer + R4_LINK_BUFFER_BYTES) return;
    for (unsigned seat = 0; seat < seats; ++seat) {
        const unsigned source_seat = seat & 1u;
        /* Copies are drawn by the HUD overlay after every view's road. */
        const int clone = seat >= 1u;
        const int dx = seat & 1u ? 160 : 0;
        const int dy = seat == 1u ? -120 : seat == 2u ? 120 : 0;
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
    unsigned x, y;
    const unsigned width = R4_LINK_VIEW_W, height = R4_LINK_VIEW_H;
    (void)seats;
    r4_link_view_origin(view, &x, &y);
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
                        const int right = slot == 3u;
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
                (void)seats;
                if (slot == 0u && x >= 225) x -= 160;
                if (slot == 1u && x < 96) x += 160;
                if (y >= 120 && y < 240) y -= 120;
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

/* Wrap this frame's HUD copies in a full-frame draw area and return the
 * packet chain's first node; its last node links to `old`. With
 * `clear_lower` it first fills the lower half black (below `clear_top`):
 * two seats leave the retail second view's leftover HUD there. Without
 * `copies` the chain is only that fill. 0 when there is nothing to draw or
 * no room. */
static uint32_t r4_link_build_hud_overlay(uint32_t old, int clear_lower,
                                          uint32_t clear_top, int copies)
{
    copies = copies && r4_link_hud_copy_head && r4_link_hud_copy_tail;
    if (!copies && !clear_lower) return 0;
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t heap = psx_mod_read_word(0x1F800000u);
    const uint32_t fill = clear_lower ? 16u : 0u;
    if ((heap & 3u) || heap < buffer ||
        heap + 24u + fill > buffer + R4_LINK_BUFFER_BYTES) return 0;
    const uint32_t y = r4_link_buffer_index() ? 240u : 0u;
    const uint32_t first = heap;
    if (clear_lower) {
        /* GP0(02h) fill: absolute VRAM, black. */
        psx_mod_write_word(heap, 0x03000000u | ((heap + 16u) & 0x00FFFFFFu));
        psx_mod_write_word(heap + 4u, 0x02000000u);
        psx_mod_write_word(heap + 8u, (y + clear_top) << 16);
        psx_mod_write_word(heap + 12u, ((240u - clear_top) << 16) | 320u);
        heap += 16u;
        if (!copies) {
            psx_mod_write_word(heap, 0x01000000u | (old & 0x00FFFFFFu));
            psx_mod_write_word(heap + 4u, 0xE1000205u);
            psx_mod_write_word(0x1F800000u, heap + 8u);
            return first;
        }
    }
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
    psx_mod_write_word(heap + 16u, 0x01000000u | (old & 0x00FFFFFFu));
    psx_mod_write_word(heap + 20u, 0xE1000205u);
    psx_mod_write_word(0x1F800000u, heap + 24u);
    return first;
}

static void r4_link_append_hud_overlay(unsigned seats)
{
    const uint32_t extra = r4_link_extra_ot_for(seats - 1u);
    if (!extra) return;
    const uint32_t head =
        r4_link_build_hud_overlay(psx_mod_read_word(extra) & 0x00FFFFFFu,
                                  0, 0, 1);
    if (head) psx_mod_write_word(extra, head & 0x00FFFFFFu);
}

/* Two seats have no private view OTs: draw the HUD copies after the last
 * node of the frame's own chain (bounded walk, like r4_link_append_ot),
 * over a black lower half. While paused (any seat count) only the two upper
 * views are drawn: the native pause menu (y 85..135) is in the frame's own
 * chain, which the lower views' private OTs would cover, so the lower half
 * is cleared from just below it instead. */
static void r4_link_append_root_hud_overlay(uint32_t root, int paused)
{
    const uint32_t buffer = psx_mod_read_word(0x800ACDCCu);
    uint32_t current = root;
    for (unsigned steps = 0; steps < 32768u; ++steps) {
        if ((current & 3u) || current < buffer ||
            current >= buffer + R4_LINK_BUFFER_BYTES) return;
        const uint32_t tag = psx_mod_read_word(current);
        const uint32_t next = tag & 0x00FFFFFFu;
        const uint32_t next_guest = 0x80000000u | next;
        if (next == 0x00FFFFFFu || next_guest < buffer ||
            next_guest >= buffer + R4_LINK_BUFFER_BYTES) {
            if (next != 0x00FFFFFFu &&
                ((next & 3u) || next >= 0x00200000u)) return;
            const uint32_t head = r4_link_build_hud_overlay(
                next, 1, paused ? 136u : R4_LINK_VIEW_H, !paused);
            if (head)
                psx_mod_write_word(current, (tag & 0xFF000000u) |
                                            (head & 0x00FFFFFFu));
            return;
        }
        current = next_guest;
    }
}

/* Some prebuilt draw-environment packets use the retail two-view clipping
 * rectangle on the second frame buffer. Rewrite the first E3/E4 pair in each
 * private view chain after all primitives have been linked, leaving shared
 * full-screen HUD packets and unrelated draw state alone. */
static int r4_link_patch_first_clip(uint32_t root, unsigned view, unsigned seats)
{
    if (view >= seats || !r4_link_seats_ok((int)seats)) return 0;
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
    if (!r4_link_seats_ok((int)seats) || view >= seats) return;
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
    return (r4_link_seats_ok(seats) && experimental &&
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
    if (pad->analog == 3u) {
        /* NeGcon (e.g. a player's Modern controls, an input transform kept
         * online): analog I is the gas, II the brake, twist the wheel. R4
         * reads the pressures calibrated on 0..106 and steers at full lock
         * 44 counts off centre (dead zone 6 + range 38). */
        const uint32_t gas = pad->rx > 106u ? 255u : (pad->rx * 255u) / 106u;
        const uint32_t brk = pad->ry > 106u ? 0x73u : (pad->ry * 0x73u) / 106u;
        int d = (int)pad->lx - 128;
        int stick = 128 + (d * 127) / 44;
        word = 0x80000000u | (gas << 8);
        if (brk > 4u) word |= (0x8Cu + brk) << 16;
        stick = stick < 0 ? 0 : (stick > 255 ? 255 : stick);
        return word | r4_link_analog_steering[stick];
    }
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
    if (!r4_link_seats_ok(seats)) return 0;
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

/* Fallback: present only this peer's quadrant of the shared frame. The pause
 * menu spans the whole frame, so a paused race shows every view. */
static void r4_link_present_local_view(unsigned seats)
{
    const int slot = psx_netplay_local_slot();
    unsigned x, y;
    if (slot < 0 || (unsigned)slot >= seats ||
        psx_mod_read_byte(R4_LINK_PAUSED) != 0) return;
    r4_link_view_origin((unsigned)slot, &x, &y);
    psx_netplay_present_local_view(x, y, R4_LINK_VIEW_W, R4_LINK_VIEW_H);
}

/* ---- Own full-screen view (psx_mod_render_local_view) -------------------
 *
 * Every peer simulates and draws the same multi-view frame (the canonical
 * frame: guest RAM, VRAM, digests). On top of that each peer redraws its own
 * seat's view as one full-screen 320x240 view inside a framework sandbox and
 * shows that instead; the sandbox restores the whole machine, so this never
 * changes guest state.
 *
 * Where: the main loop's VSync(0) entry (0x8008B330, ra 0x8001E7E4), as R4's
 * frame-rate plugin does. The link handler has built tick n (cars, cameras,
 * OT), the previous OT has drawn the other buffer (DrawSync returned), and
 * the display is about to flip to that buffer, whose RAM and VRAM are free
 * to redraw.
 *
 * What: the link handler's own one-view path (two entrants, retail: one
 * console, one full-screen car; 0x80115D80..0x80116254 with view flag 0 and
 * one view), draw calls only. The seat's car takes the P1 object's place
 * (that path draws P1, 0x800AC0B0), its lap time takes slot 0, and its
 * camera (already advanced by tick n's handler) is the view's camera. Logic
 * in that path (the camera step, the countdown fade counter, engine audio,
 * the packet send) is left out. */
#define R4_LINK_P1_CAR 0x800AC0B0u
#define R4_LINK_CAR_BYTES 0x320u
#define R4_LINK_LAP_TIMES 0x800F4E20u
#define R4_LINK_PHASE 0x800FF860u
#define R4_LINK_TICK 0x800F2F94u
#define R4_LINK_VSYNC 0x8008B330u
#define R4_LINK_RA_PASS_POINT 0x8001E7E4u
#define R4_LINK_RA_NESTED 0x8001E7E8u
#define R4_LINK_BUF_BASE 0x800ADCA0u
#define R4_LINK_FRAME_COUNTER 0x800AC064u
#define R4_LINK_CUR_BUF 0x800ACDCCu
#define R4_LINK_BUF_INDEX 0x800F4DA0u
#define R4_LINK_OT2_ENABLED 0x800ACDB0u

static int r4_link_in_local_view;
#define R4_LINK_MIRROR_SLIDE 0x800F4E90u
static int32_t r4_link_mirror_slide, r4_link_mirror_next;
static uint32_t r4_link_mirror_tick = UINT32_MAX;
static uint64_t r4_link_local_ok, r4_link_local_failed;

typedef struct R4LinkCall {
    struct CPUState *cpu;
    uint32_t sp;
    int broken;
} R4LinkCall;

static uint32_t r4_link_call(R4LinkCall *c, uint32_t fn, uint32_t a0,
                             uint32_t a1)
{
    struct CPUState *cpu = c->cpu;
    if (c->broken) return 0;
    cpu->gpr[4] = a0;
    cpu->gpr[5] = a1;
    cpu->gpr[6] = cpu->gpr[7] = 0;
    cpu->gpr[29] = c->sp;
    cpu->gpr[31] = R4_LINK_RA_NESTED;
    psx_dispatch_call(cpu, fn, R4_LINK_RA_NESTED);
    if (cpu->gpr[29] != c->sp) c->broken = 1; /* left its frame: discard */
    return cpu->gpr[2];
}

/* The handler's km/h conversion for the speed HUD (0x80115EE0..F14). */
static uint32_t r4_link_kmh(uint32_t car)
{
    const int32_t a0 = (int32_t)(int16_t)psx_mod_read_half(car + 0x1D8u) * 160;
    const int32_t hi = (int32_t)(((int64_t)a0 * (int32_t)0xE070381Du) >> 32);
    return (uint32_t)(((hi + a0) >> 10) - (a0 >> 31));
}

static void r4_link_swap_bytes(uint32_t a, uint32_t b, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; i += 4u) {
        const uint32_t x = psx_mod_read_word(a + i);
        psx_mod_write_word(a + i, psx_mod_read_word(b + i));
        psx_mod_write_word(b + i, x);
    }
}

/* Seat k's car and its camera blocks (angle s6, position s7 in the view
 * loop): seats 0/1 use the retail P1/P2 objects and camera pairs, seats 2/3
 * their roster entries and the bridge's extra cameras. */
static int r4_link_seat_view(unsigned seat, uint32_t *car, uint32_t *angle,
                             uint32_t *position)
{
    if (seat < 2u) {
        *car = R4_LINK_P1_CAR + R4_LINK_CAR_BYTES * seat;
        *angle = 0x80118FB8u + 16u * seat;
        *position = 0x80118FD8u + 16u * seat;
        return 1;
    }
    if (!r4_link_extra_view_cameras || seat > 3u) return 0;
    *car = psx_mod_read_word(0x800FFDD0u + 4u * seat);
    *angle = r4_link_extra_view_cameras + 32u * (seat - 2u);
    *position = *angle + 16u;
    return (*car & 0xFFE00003u) == 0x80000000u;
}

typedef struct R4LinkLocalView {
    unsigned seat;
    uint32_t buffer, buffer_index;
} R4LinkLocalView;

static int r4_link_draw_local_view(struct CPUState *cpu, void *user,
                                   uint32_t alpha_q16)
{
    const R4LinkLocalView *v = (const R4LinkLocalView *)user;
    const uint32_t buf = v->buffer, ot = buf + 0x70u;
    const uint32_t phase = psx_mod_read_word(R4_LINK_PHASE);
    const uint32_t tick = psx_mod_read_word(R4_LINK_TICK);
    uint32_t car_k, angle, position;
    R4LinkCall call = {cpu, (cpu->gpr[29] - 0x100u) & ~7u, 0};
    (void)alpha_q16;
    if (!r4_link_seat_view(v->seat, &car_k, &angle, &position)) return 0;

    /* The other buffer is the draw target everywhere the code looks. */
    psx_mod_write_word(R4_LINK_CUR_BUF, buf);
    psx_mod_write_word(R4_LINK_BUF_INDEX, v->buffer_index);
    psx_mod_write_word(0x1F800004u, ot);
    psx_mod_write_word(0x1F800000u, buf + 0x1670u);
    r4_link_call(&call, 0x80093418u, ot, 0x2C0u);          /* ClearOTagR */
    r4_link_call(&call, 0x80093418u, buf + 0xB70u, 0x2C0u);
    /* One view (two entrants): view flag 0, the one-view OT, no LOD flag,
     * and the one-view HUD the race init would have set up for two
     * entrants: its rank sprites (0x8002094C) and the speed/gear/needle
     * layout (0x80021014), both called as 0x8003D328..34 does. */
    const uint16_t entrants = psx_mod_read_half(R4_LINK_ENTRANTS);
    r4_link_call(&call, 0x8002094Cu, 4u, 0);
    psx_mod_write_half(R4_LINK_ENTRANTS, 2u);
    psx_mod_write_half(0x1F80005Eu, 0u);
    r4_link_call(&call, 0x80021014u, 4u, 0);
    /* The seat's car in the P1 object, its lap time in slot 0. The roster
     * still lists every car, so every car is drawn where it is. */
    if (v->seat) {
        r4_link_swap_bytes(R4_LINK_P1_CAR, car_k, R4_LINK_CAR_BYTES);
        const uint16_t lap0 = psx_mod_read_half(R4_LINK_LAP_TIMES);
        psx_mod_write_half(R4_LINK_LAP_TIMES,
                           psx_mod_read_half(R4_LINK_LAP_TIMES + 2u * v->seat));
        psx_mod_write_half(R4_LINK_LAP_TIMES + 2u * v->seat, lap0);
    }
    const uint32_t car = R4_LINK_P1_CAR;

    if (phase < 4u)
        r4_link_call(&call, 0x80034444u, ot, tick);
    if (phase - 1u < 3u) {
        /* HUD for one view (0x80115E84..F44, s4 = 1, s1 = 0, s2 = 1). */
        r4_link_call(&call, 0x800212E0u,
                     (uint32_t)(int32_t)(int16_t)psx_mod_read_half(R4_LINK_LAP_TIMES), 0);
        r4_link_call(&call, 0x80021DDCu, 0, 0);
        r4_link_call(&call, 0x80021C38u, 0,
                     (uint32_t)(int32_t)(int16_t)psx_mod_read_half(car + 0x2AAu));
        r4_link_call(&call, 0x80022048u, ot, 0);
        r4_link_call(&call, 0x80021614u, r4_link_kmh(car), 0);
        r4_link_call(&call, 0x80021960u,
                     (uint32_t)(int32_t)(int16_t)psx_mod_read_half(car + 0x27Au), 0);
        /* Rank "n | N": N is the entrant count (0x80021A90). */
        psx_mod_write_half(R4_LINK_ENTRANTS, entrants);
        r4_link_call(&call, 0x80021A20u,
                     (uint32_t)(int32_t)(int16_t)psx_mod_read_half(car + 0x1EEu), 0);
        psx_mod_write_half(R4_LINK_ENTRANTS, 2u);
        r4_link_call(&call, 0x8002C194u, psx_mod_read_word(0x800AC75Cu), 0);
        r4_link_call(&call, 0x80021BA4u, 1, 0);
        r4_link_call(&call, 0x80021134u, 0, 0);
        r4_link_call(&call, 0x80020A98u, 3, 0);
    }
    if ((int16_t)psx_mod_read_half(car + 0x1E8u) != -1) {
        /* The view (0x80115FB8..0x80116150, s1 = 0): its camera blocks into
         * scratch, full-screen viewport 0, camera, cars, course, mirror. */
        for (unsigned i = 0; i < 4u; ++i) {
            psx_mod_write_word(0x1F800018u + 4u * i,
                               psx_mod_read_word(angle + 4u * i));
            psx_mod_write_word(0x1F800008u + 4u * i,
                               psx_mod_read_word(position + 4u * i));
        }
        r4_link_call(&call, 0x8006F2B0u, 0, 0);
        /* The camera step (0x80116024..5C) derives this view's matrices and
         * course position from its blocks. The shared frame already took
         * this tick's step for the seat, so the image leads by at most one
         * smoothing step; the sandbox discards the advanced blocks. */
        if (phase == 0u && tick < 90u)
            r4_link_call(&call, 0x80033974u, car, 0);
        else
            r4_link_call(&call, 0x800340ECu, car, 0);
        r4_link_call(&call, 0x8006E4D0u, 0, 0);
        const uint32_t side = psx_mod_read_half(0x800AD6C0u) ^ 1u;
        r4_link_call(&call, 0x8002E554u, 2u * side + 2u, 0); /* 0x801160C4 */
        if ((int16_t)psx_mod_read_half(car + 0x1EAu) != 0 &&
            psx_mod_read_byte(R4_LINK_PAUSED) == 0)
            r4_link_call(&call, 0x8002258Cu, 0, 0);
        /* The course renderer keeps state per view index; this seat's view
         * used index seat & 1 in the shared frame. */
        r4_link_call(&call, 0x8006F00Cu, v->seat & 1u, 0);
        r4_link_call(&call, 0x800738C4u, 0, 0);
        r4_link_call(&call, 0x80074AD8u, 0, 0);
        r4_link_call(&call, 0x800374F8u, 1, 0);
        /* The rear-view mirror (one view only) slides in by one step per
         * call (0x800F4E90). The shared frame never draws it, so the slide
         * is this peer's presentation state, kept on the host. */
        psx_mod_write_word(R4_LINK_MIRROR_SLIDE, (uint32_t)r4_link_mirror_slide);
        r4_link_call(&call, 0x80070A58u, psx_mod_read_word(0x800AC068u), 0);
        r4_link_mirror_next = (int32_t)psx_mod_read_word(R4_LINK_MIRROR_SLIDE);
    }
    r4_link_call(&call, 0x80093590u, buf, 0);               /* PutDrawEnv */
    r4_link_call(&call, 0x80093520u, buf + 0xB6Cu, 0);      /* DrawOTag */
    if (psx_mod_read_word(R4_LINK_OT2_ENABLED))
        r4_link_call(&call, 0x80093520u, buf + 0x166Cu, 0);
    return !call.broken;
}

/* At the pass point of a race frame: draw this peer's own view, or fall back
 * to its quadrant of the shared frame. */
static void r4_link_local_view(struct CPUState *cpu)
{
    const int seats = psx_netplay_seat_count();
    const int slot = psx_netplay_local_slot();
    const uint32_t phase = psx_mod_read_word(R4_LINK_PHASE);
    R4LinkLocalView view;
    PSXModRenderPass rect;
    if (!r4_link_seats_ok(seats) || slot < 0 || slot >= seats ||
        psx_mod_read_byte(R4_LINK_PAUSED) != 0)
        return; /* paused: the whole frame with the shared menu */
    view.seat = (unsigned)slot;
    view.buffer_index = (psx_mod_read_word(R4_LINK_FRAME_COUNTER) & 1u) ^ 1u;
    view.buffer = R4_LINK_BUF_BASE + view.buffer_index * R4_LINK_BUFFER_BYTES;
    const uint32_t status = psx_mod_render_local_view_status();
    if (phase < 4u && status == PSX_MOD_RENDER_PASS_READY &&
        !getenv("PSX_R4_LINK_QUADRANT_VIEW")) {
        memset(&rect, 0, sizeof rect);
        rect.struct_size = sizeof rect;
        rect.y = (uint16_t)(view.buffer_index ? 240u : 0u);
        rect.w = 320;
        rect.h = 240;
        const uint32_t tick = psx_mod_read_word(R4_LINK_TICK);
        if (r4_link_mirror_tick == UINT32_MAX || tick < r4_link_mirror_tick)
            r4_link_mirror_slide =
                (int32_t)psx_mod_read_word(R4_LINK_MIRROR_SLIDE); /* new race */
        r4_link_mirror_tick = tick;
        r4_link_mirror_next = r4_link_mirror_slide;
        r4_link_in_local_view = 1;
        const int ok = psx_mod_render_local_view(cpu, &rect,
                                                 r4_link_draw_local_view, &view);
        r4_link_in_local_view = 0;
        if (ok) {
            r4_link_mirror_slide = r4_link_mirror_next;
            ++r4_link_local_ok;
            return;
        }
        if (++r4_link_local_failed <= 4)
            fprintf(stderr, "r4 link: own view refused or rolled back "
                    "(status %u); showing this seat's quadrant\n",
                    psx_mod_render_local_view_status());
    } else if (status == PSX_MOD_RENDER_PASS_SESSION) {
        return; /* resimulation: nothing is presented */
    }
    r4_link_present_local_view((unsigned)seats);
}

static int r4_link_serial_filter(struct CPUState *cpu, uint32_t address)
{
    /* Guest calls made by the own-view draw run in a sandbox; none of the
     * bridge's frame edits apply to them. */
    if (r4_link_in_local_view) return 0;
    if (address == R4_LINK_VSYNC) {
        if (cpu->gpr[31] == R4_LINK_RA_PASS_POINT && r4_link_ready() &&
            r4_link_views_frame)
            r4_link_local_view(cpu);
        return 0;
    }
    if (((address == 0x80021614u && cpu->gpr[31] == 0x80115F18u) ||
         (address == 0x80021960u && cpu->gpr[31] == 0x80115F24u)) &&
        r4_link_ready() && cpu->gpr[5] >= 2u && cpu->gpr[5] < 4u)
        cpu->gpr[5] &= 1u; /* valid HUD record, unchanged per-car value a0 */
    if (address == 0x8006F00Cu && r4_link_ready() &&
        r4_link_draw_view >= 2u && r4_link_draw_view < 4u)
        cpu->gpr[4] &= 1u; /* course renderer has only two view indices */
    if (address == 0x801157ECu && r4_link_ready() &&
        psx_netplay_seat_count() == 2) {
        /* Two entrants select the retail one-view layout, which draws only
         * the first car. Take the two-view branch (0x801157D0..E8) instead,
         * so every peer draws both seats' views: view flag 2, the two-view
         * OT base, and its LOD flag. The entrant count itself stays 2. */
        psx_mod_write_word(cpu->gpr[29] + 40u, 2u);
        psx_mod_write_word(cpu->gpr[29] + 44u,
                           psx_mod_read_word(0x800ACDCCu) + 0xB70u);
        psx_mod_write_half(0x1F80005Eu, 1u);
    }
    if (r4_link_ready() && psx_netplay_seat_count() == 2) {
        /* With exactly two entrants the HUD setup (0x8002094C, 0x80021014
         * at race init 0x8003D328) and the per-frame HUD OT pass
         * (0x80020A98 from 0x80115F7C) build the one-view HUD. Both peers
         * draw two views, so let those calls see the two-view entrant count
         * and restore the real count (2) right after them; rank, results and
         * the race simulation keep reading 2. Guest-only, so peers agree. */
        if ((address == 0x8002094Cu && cpu->gpr[31] == 0x8003D330u &&
             cpu->gpr[4] == 4u) ||
            (address == 0x80021134u && cpu->gpr[31] == 0x80115F7Cu))
            psx_mod_write_half(R4_LINK_ENTRANTS, 4u);
        if ((address == 0x8007807Cu && cpu->gpr[31] == 0x8003D3ACu) ||
            address == 0x80115F84u)
            psx_mod_write_half(R4_LINK_ENTRANTS, 2u);
    }
    if (address == 0x80093520u && r4_link_ready() && r4_link_views_frame) {
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
            const int paused = psx_mod_read_byte(R4_LINK_PAUSED) != 0;
            if (seats == 2u || paused)
                r4_link_append_root_hud_overlay(cpu->gpr[4], paused);
            else if (r4_link_append_ot(cpu->gpr[4], seats))
                r4_link_append_hud_overlay(seats);
            r4_link_views_frame = 0;
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
    if (address == 0x80115770u && r4_link_ready()) {
        const int seats = psx_netplay_seat_count();
        if (r4_link_seats_ok(seats)) {
            r4_link_views_frame = 1;
            r4_link_draw_view = 0;
            for (unsigned i = 0; i < 4u; ++i)
                r4_link_hud_start[i] = r4_link_hud_end[i] = 0;
            r4_link_hud_open = 4;
            r4_link_hud_copy_head = r4_link_hud_copy_tail = 0;
            r4_link_patch_view_loop((unsigned)seats);
            r4_link_clear_extra_ots();
        }
    }
    if (address == 0x80115F84u && r4_link_ready())
        r4_link_prepare_viewport((unsigned)psx_netplay_seat_count(), 0);
    if (r4_link_ready()) {
        if (address == 0x80021774u && cpu->gpr[5] < 4u)
            r4_link_hud_boundary(cpu->gpr[5]);
        if (address == 0x80021134u)
            r4_link_hud_boundary(4);
    }
    if (address == 0x80115FA8u && r4_link_ready()) {
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
        /* The serial handshake normally reports accepted entrants: one car
         * per seat, in seat order; the remaining slots stay inactive. */
        for (unsigned slot = 0; slot < 4; ++slot)
            psx_mod_write_byte(0x800AC074u + slot,
                               (int)slot < psx_netplay_seat_count() ? 1 : 0);
        psx_mod_write_half(0x800FF838u, 2);
        return 0;
    }
    if ((address & 0x1FFFFFFFu) == 0x00115770u &&
        psx_netplay_seat_count() < 4 && r4_link_ready() &&
        psx_mod_read_half(0x800AC754u) == 4 &&
        psx_mod_read_byte(0x800AC074u + (unsigned)psx_netplay_seat_count()) == 0) {
        /* The original pair-local roster reserves four slots. Once R4 has
         * initialized the accepted cars (the first seat-count slots),
         * exclude the empty slots from race loops and ranking counts. */
        psx_mod_write_half(0x800AC754u, (uint16_t)psx_netplay_seat_count());
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
        0x801157ECu, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x8002094Cu, r4_link_serial_filter);
    (void)psx_game_register_netplay_function_filter(
        0x8007807Cu, r4_link_serial_filter);
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
    (void)psx_game_register_netplay_function_filter(
        R4_LINK_VSYNC, r4_link_serial_filter);
}
