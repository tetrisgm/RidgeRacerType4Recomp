/* r4_widescreen_hud.h - which R4 packets are HUD, and which screen edge they
 * belong to.
 *
 * Pure helpers (no runtime dependencies) so tests/test_r4_widescreen.c can
 * check them on synthetic packets.
 *
 * R4's HUD writers append GPU packets to the frame's primitive heap through
 * the scratchpad pointer *(0x1F800000) and link them into the ordering table.
 * The plugin brackets each producer's heap range (the producer's entry opens
 * it, the next producer or a known non-HUD draw call closes it), then walks
 * the range here and tags every drawing command with its screen edge. The
 * native-wide compositor moves tagged commands by the live reveal, so the
 * HUD sits at the true window edges while the 3D view widens.
 *
 * The producer list and its left/right/centre/auto classes, and the auto
 * thresholds, are derived from dogewow2048's analysis of R4 (JP) for their
 * "60 FPS + 16:9" patch, re-mapped to the US executable. Credit to
 * dogewow2048; see docs/WIDESCREEN.md.
 */
#ifndef R4_WIDESCREEN_HUD_H
#define R4_WIDESCREEN_HUD_H

#include <stdint.h>

enum {
    R4_HUD_CENTRE = 0,   /* stays centred: not tagged */
    R4_HUD_LEFT = 1,     /* anchored to the left edge (-1) */
    R4_HUD_RIGHT = 2,    /* anchored to the right edge (+1) */
    R4_HUD_AUTO = 3      /* per command, by its screen X */
};

typedef struct {
    uint32_t fn;         /* US function entry */
    uint8_t cls;
} R4HudProducer;

#define R4_HUD_PRODUCER_COUNT 18
static const R4HudProducer r4_hud_producers[R4_HUD_PRODUCER_COUNT] = {
    { 0x8003C4B4u, R4_HUD_AUTO },    /*  0 split-time popup: icon + digits */
    { 0x80021DDCu, R4_HUD_LEFT },    /*  1 course minimap */
    { 0x8002BEA4u, R4_HUD_RIGHT },   /*  2 time / lap figures */
    { 0x800212E0u, R4_HUD_RIGHT },   /*  3 tachometer needle */
    { 0x80021134u, R4_HUD_RIGHT },   /*  4 gauge frame */
    { 0x80021614u, R4_HUD_RIGHT },   /*  5 speed (US mph variant) */
    { 0x80021960u, R4_HUD_RIGHT },   /*  6 rpm element */
    { 0x8002C0F8u, R4_HUD_RIGHT },   /*  7 text at x=273 + static sprite */
    { 0x8002C194u, R4_HUD_RIGHT },   /*  8 time readout */
    { 0x80021774u, R4_HUD_RIGHT },   /*  9 multi-digit number */
    { 0x80021C38u, R4_HUD_RIGHT },   /* 10 cluster sprites */
    { 0x80034444u, R4_HUD_CENTRE },  /* 11 centre band + text */
    { 0x80034F14u, R4_HUD_CENTRE },  /* 12 sprite stack at x=144 (pause) */
    { 0x80039C04u, R4_HUD_AUTO },    /* 13 popup */
    { 0x8002258Cu, R4_HUD_AUTO },    /* 14 104x16 banner */
    { 0x80022048u, R4_HUD_AUTO },    /* 15 misc HUD */
    { 0x800559BCu, R4_HUD_CENTRE },  /* 16 wrapper of 17 (demo/replay) */
    { 0x80055968u, R4_HUD_CENTRE },  /* 17 centre element */
};

/* Class of a producer entry, or -1 if `fn` is not one. */
static inline int r4_hud_class_of(uint32_t fn)
{
    for (int i = 0; i < R4_HUD_PRODUCER_COUNT; i++)
        if (r4_hud_producers[i].fn == fn) return r4_hud_producers[i].cls;
    return -1;
}

/* Auto class: left third-ish -> left edge, right -> right edge, else centre.
 * x is the command's anchor on the 320-pixel screen. */
static inline int r4_hud_edge_for_x(int32_t x)
{
    if (x < 96) return -1;
    if (x < 225) return 0;
    return 1;
}

/* ---- GP0 command shapes --------------------------------------------------- */

#define R4_GP0_VARIABLE 0xFFFFFFFFu  /* polyline: length found by its terminator */

/* Words in one GP0 command (the command word included), R4_GP0_VARIABLE for a
 * polyline, or 0 for a CPU->VRAM transfer (image data follows: stop). */
static inline uint32_t r4_gp0_words(uint32_t cmd_word)
{
    uint32_t c = cmd_word >> 24;
    if (c >= 0x20 && c < 0x40) {                 /* polygon */
        uint32_t verts = (c & 0x08) ? 4u : 3u;
        uint32_t textured = (c & 0x04) ? 1u : 0u;
        uint32_t gouraud = (c & 0x10) ? 1u : 0u;
        return 1u + verts * (1u + textured) + gouraud * (verts - 1u);
    }
    if (c >= 0x40 && c < 0x60) {                 /* line / polyline */
        if (c & 0x08) return R4_GP0_VARIABLE;
        return (c & 0x10) ? 4u : 3u;
    }
    if (c >= 0x60 && c < 0x80) {                 /* rectangle */
        uint32_t textured = (c & 0x04) ? 1u : 0u;
        uint32_t variable = ((c >> 3) & 3u) == 0u ? 1u : 0u;
        return 2u + textured + variable;
    }
    if (c == 0x02) return 3u;                    /* fill */
    if (c >= 0x80 && c < 0xA0) return 4u;        /* VRAM -> VRAM */
    if (c >= 0xA0 && c < 0xC0) return 0u;        /* CPU -> VRAM + data */
    if (c >= 0xC0 && c < 0xE0) return 3u;        /* VRAM -> CPU */
    return 1u;                                   /* nop, cache, E1..E6 */
}

/* True for commands that draw at a screen position (the ones a HUD tag can
 * move). */
static inline int r4_gp0_draws(uint32_t cmd_word)
{
    uint32_t c = cmd_word >> 24;
    return c >= 0x20 && c < 0x80;
}

typedef uint32_t (*R4ReadWord)(uint32_t address);

/* The X that classifies a command: vertex 0 for polygons, lines and sprites,
 * the centre for untextured rectangles (tiles). */
static inline int32_t r4_gp0_anchor_x(R4ReadWord rd, uint32_t cmd_addr,
                                      uint32_t cmd_word)
{
    uint32_t c = cmd_word >> 24;
    int32_t x = (int16_t)(rd(cmd_addr + 4u) & 0xFFFFu);
    if (c >= 0x60 && c < 0x80 && !(c & 0x04)) {
        uint32_t size = (c >> 3) & 3u;
        int32_t w = size == 1u ? 1 : size == 2u ? 8 : size == 3u ? 16
                  : (int32_t)(rd(cmd_addr + 8u) & 0xFFFFu);
        x += w / 2;
    }
    return x;
}

typedef void (*R4HudVisit)(void *ctx, uint32_t key, int edge);

/* A bracketed range never legitimately exceeds this; a larger one means the
 * bracketing saw something unexpected, and nothing is tagged. */
#define R4_HUD_WALK_MAX_BYTES 0x10000u

/* Walk the packets in [start, end) (P_TAG: length in the top byte, words
 * follow) and call visit(ctx, key, edge) for every drawing command whose
 * edge is nonzero. key is the command address minus 4, the identity
 * psx_mod_tag_hud_primitive expects (for the first command, the P_TAG).
 * Returns the number of commands visited, or -1 for a range that cannot be a
 * packet run (then nothing is visited). */
static inline int r4_hud_walk(R4ReadWord rd, uint32_t start, uint32_t end,
                              int cls, R4HudVisit visit, void *ctx)
{
    int visited = 0;
    if (cls == R4_HUD_CENTRE || end == start) return 0;
    if (end < start || end - start > R4_HUD_WALK_MAX_BYTES ||
        ((start | end) & 3u))
        return -1;
    for (uint32_t p = start; p < end;) {
        uint32_t len = rd(p) >> 24;
        uint32_t pend = p + 4u + 4u * len;
        if (pend > end) pend = end;               /* never read past the heap top */
        for (uint32_t q = p + 4u; q < pend;) {
            uint32_t w0 = rd(q);
            uint32_t words = r4_gp0_words(w0);
            if (words == R4_GP0_VARIABLE) {        /* polyline: find 0x5xxx5xxx */
                uint32_t t = q + 8u;
                while (t < pend && (rd(t) & 0xF000F000u) != 0x50005000u) t += 4u;
                words = (t - q) / 4u + 1u;
            }
            if (words == 0u) break;                /* image data: rest of packet */
            if (r4_gp0_draws(w0)) {
                int edge = cls == R4_HUD_LEFT ? -1 : cls == R4_HUD_RIGHT ? 1
                         : r4_hud_edge_for_x(r4_gp0_anchor_x(rd, q, w0));
                if (edge) {
                    visit(ctx, q - 4u, edge);
                    visited++;
                }
            }
            q += 4u * words;
        }
        p += 4u + 4u * len;
    }
    return visited;
}

#endif /* R4_WIDESCREEN_HUD_H */
