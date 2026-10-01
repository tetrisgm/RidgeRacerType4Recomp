/* Unit tests for the pure helpers behind r4.enhancement.widescreen:
 * the View option, the HUD class table and packet walker, the race
 * predicate, and the course-list union. No game data or runtime needed. */
#include "r4_widescreen_hud.h"
#include "r4_pvs.h"
#include "r4_widescreen_scene.h"
#include "r4_widescreen_view.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } } while (0)

/* ---- fake guest RAM ---------------------------------------------------- */
#define RAM_BASE 0x80000000u
#define RAM_SIZE 0x200000u
static uint32_t ram[RAM_SIZE / 4];
static uint32_t rd(uint32_t a)
{
    if (a < RAM_BASE || a - RAM_BASE >= RAM_SIZE) return 0xDEADBEEFu;
    return ram[(a - RAM_BASE) >> 2];
}
static uint16_t rdh(uint32_t a)
{
    uint32_t w = rd(a & ~3u);
    return (uint16_t)((a & 2u) ? w >> 16 : w & 0xFFFFu);
}
static void wr(uint32_t a, uint32_t v) { ram[(a - RAM_BASE) >> 2] = v; }
static void wrh(uint32_t a, uint16_t v)
{
    uint32_t w = rd(a & ~3u);
    w = (a & 2u) ? (w & 0xFFFFu) | ((uint32_t)v << 16) : (w & 0xFFFF0000u) | v;
    wr(a & ~3u, w);
}

static void test_view(void)
{
    R4WidescreenView v = r4_widescreen_view("Fit");
    CHECK(v.numerator == 16 && v.denominator == 9 && v.fit, "Fit starts at 16:9 and follows the window");
    v = r4_widescreen_view("16:9");
    CHECK(v.numerator == 16 && v.denominator == 9 && !v.fit, "16:9 is fixed");
    v = r4_widescreen_view("21:9");
    CHECK(v.numerator == 21 && !v.fit, "21:9 is fixed");
    v = r4_widescreen_view("32:9");
    CHECK(v.numerator == 32 && !v.fit, "32:9 is fixed");
    v = r4_widescreen_view("48:9");
    CHECK(v.fit && v.numerator == 16, "an unknown choice falls back to Fit");
    v = r4_widescreen_view(NULL);
    CHECK(v.fit, "no choice falls back to Fit");
}

static void test_class_table(void)
{
    int left = 0, right = 0, centre = 0, autos = 0;
    for (int i = 0; i < R4_HUD_PRODUCER_COUNT; i++) {
        int c = r4_hud_producers[i].cls;
        left += c == R4_HUD_LEFT;
        right += c == R4_HUD_RIGHT;
        centre += c == R4_HUD_CENTRE;
        autos += c == R4_HUD_AUTO;
        for (int j = 0; j < i; j++)
            CHECK(r4_hud_producers[j].fn != r4_hud_producers[i].fn, "producers are unique");
    }
    CHECK(left == 1 && right == 9 && centre == 4 && autos == 4, "18 producers: 1 left, 9 right, 4 centre, 4 auto");
    CHECK(r4_hud_class_of(0x80021DDCu) == R4_HUD_LEFT, "minimap is left");
    CHECK(r4_hud_class_of(0x80021614u) == R4_HUD_RIGHT, "US speed readout is right");
    CHECK(r4_hud_class_of(0x80034F14u) == R4_HUD_CENTRE, "pause stack is centre");
    CHECK(r4_hud_class_of(0x8003C4B4u) == R4_HUD_AUTO, "split popup is auto");
    CHECK(r4_hud_class_of(0x80093520u) == -1, "DrawOTag is not a producer");
}

static void test_auto_thresholds(void)
{
    CHECK(r4_hud_edge_for_x(-10) == -1, "off-left is left");
    CHECK(r4_hud_edge_for_x(95) == -1, "x 95 is left");
    CHECK(r4_hud_edge_for_x(96) == 0, "x 96 is centre");
    CHECK(r4_hud_edge_for_x(224) == 0, "x 224 is centre");
    CHECK(r4_hud_edge_for_x(225) == 1, "x 225 is right");
}

static void test_gp0_words(void)
{
    CHECK(r4_gp0_words(0x20000000u) == 4, "F3");
    CHECK(r4_gp0_words(0x2C000000u) == 9, "FT4");
    CHECK(r4_gp0_words(0x30000000u) == 6, "G3");
    CHECK(r4_gp0_words(0x3C000000u) == 12, "GT4");
    CHECK(r4_gp0_words(0x28000000u) == 5, "F4");
    CHECK(r4_gp0_words(0x40000000u) == 3, "line");
    CHECK(r4_gp0_words(0x50000000u) == 4, "gouraud line");
    CHECK(r4_gp0_words(0x48000000u) == R4_GP0_VARIABLE, "polyline");
    CHECK(r4_gp0_words(0x60000000u) == 3, "variable tile");
    CHECK(r4_gp0_words(0x64000000u) == 4, "variable sprite");
    CHECK(r4_gp0_words(0x68000000u) == 2, "1x1 tile");
    CHECK(r4_gp0_words(0x7C000000u) == 3, "16x16 sprite");
    CHECK(r4_gp0_words(0xE1000000u) == 1, "draw mode");
    CHECK(r4_gp0_words(0x02000000u) == 3, "fill");
    CHECK(r4_gp0_words(0x80000000u) == 4, "vram copy");
    CHECK(r4_gp0_words(0xA0000000u) == 0, "cpu->vram stops the walk");
    CHECK(!r4_gp0_draws(0xE1000000u) && r4_gp0_draws(0x64000000u), "only prims draw");
}

typedef struct {
    uint32_t keys[64];
    int edges[64];
    int n;
} Visits;
static void visit(void *ctx, uint32_t key, int edge)
{
    Visits *v = (Visits *)ctx;
    if (v->n < 64) { v->keys[v->n] = key; v->edges[v->n] = edge; }
    v->n++;
}

/* Heap at 0x80100000:
 *   p0: SPRT (4 words) at x=273            -> right
 *   p1: DR_MODE + SPRT at x=8 (5 words)    -> command 2 only, left
 *   p2: TILE 0x60 x=150 w=40 (centre 170)  -> centre (not visited under auto)
 *   p3: TILE 0x60 x=60 w=80 (centre 100)   -> centre
 *   p4: F4 poly at x=300                   -> right
 *   p5: polyline x=10 ... terminator       -> left
 *   p6: empty packet (len 0) */
static uint32_t build_heap(uint32_t p)
{
    wr(p + 0, 0x04000000u); wr(p + 4, 0x64808080u); wr(p + 8, (20u << 16) | 273u);
    wr(p + 12, 0); wr(p + 16, (12u << 16) | 12u);
    p += 20;                                                    /* p1 */
    wr(p + 0, 0x05000000u); wr(p + 4, 0xE1000200u);
    wr(p + 8, 0x64808080u); wr(p + 12, (174u << 16) | 8u); wr(p + 16, 0); wr(p + 20, (8u << 16) | 8u);
    p += 24;                                                    /* p2 */
    wr(p + 0, 0x03000000u); wr(p + 4, 0x60000000u); wr(p + 8, (100u << 16) | 150u); wr(p + 12, (4u << 16) | 40u);
    p += 16;                                                    /* p3 */
    wr(p + 0, 0x03000000u); wr(p + 4, 0x60000000u); wr(p + 8, (100u << 16) | 60u); wr(p + 12, (4u << 16) | 80u);
    p += 16;                                                    /* p4 */
    wr(p + 0, 0x05000000u); wr(p + 4, 0x28FFFFFFu); wr(p + 8, (10u << 16) | 300u);
    wr(p + 12, (10u << 16) | 310u); wr(p + 16, (20u << 16) | 300u); wr(p + 20, (20u << 16) | 310u);
    p += 24;                                                    /* p5 */
    wr(p + 0, 0x05000000u); wr(p + 4, 0x48FFFFFFu); wr(p + 8, (5u << 16) | 10u);
    wr(p + 12, (6u << 16) | 20u); wr(p + 16, (7u << 16) | 30u); wr(p + 20, 0x55555555u);
    p += 24;                                                    /* p6 */
    wr(p + 0, 0x00000000u);
    p += 4;
    return p;
}

static void test_walker(void)
{
    const uint32_t h = 0x80100000u;
    uint32_t end = build_heap(h);
    Visits v;

    memset(&v, 0, sizeof v);
    int n = r4_hud_walk(rd, h, end, R4_HUD_AUTO, visit, &v);
    CHECK(n == 4 && v.n == 4, "auto walk visits the four edge commands");
    CHECK(v.keys[0] == h && v.edges[0] == 1, "p0 sprite: key is the P_TAG, right");
    CHECK(v.keys[1] == h + 20 + 4 && v.edges[1] == -1, "p1 second command: key is command-4, left");
    CHECK(v.keys[2] == h + 20 + 24 + 16 + 16 && v.edges[2] == 1, "p4 polygon is right");
    CHECK(v.keys[3] == h + 20 + 24 + 16 + 16 + 24 && v.edges[3] == -1, "p5 polyline is left");

    memset(&v, 0, sizeof v);
    n = r4_hud_walk(rd, h, end, R4_HUD_RIGHT, visit, &v);
    CHECK(n == 6, "fixed class tags every drawing command (not DR_MODE)");
    for (int i = 0; i < v.n && i < 64; i++) CHECK(v.edges[i] == 1, "fixed right edge");

    memset(&v, 0, sizeof v);
    CHECK(r4_hud_walk(rd, h, end, R4_HUD_CENTRE, visit, &v) == 0 && v.n == 0, "centre never tags");
    CHECK(r4_hud_walk(rd, h, h, R4_HUD_AUTO, visit, &v) == 0, "empty range");
    CHECK(r4_hud_walk(rd, end, h, R4_HUD_AUTO, visit, &v) == -1, "reversed range is rejected");
    CHECK(r4_hud_walk(rd, h, h + 0x20000u, R4_HUD_AUTO, visit, &v) == -1, "oversized range is rejected");
    CHECK(r4_hud_walk(rd, h + 2, end, R4_HUD_AUTO, visit, &v) == -1, "unaligned range is rejected");
    CHECK(v.n == 0, "rejected ranges visit nothing");

    /* A packet whose length runs past the range end is clipped to it. */
    memset(&v, 0, sizeof v);
    n = r4_hud_walk(rd, h, h + 12, R4_HUD_RIGHT, visit, &v);
    CHECK(n == 1, "a truncated packet still visits its first command");

    /* Image data inside a packet stops that packet only. */
    const uint32_t g = 0x80110000u;
    wr(g + 0, 0x05000000u); wr(g + 4, 0xA0000000u); wr(g + 8, 0); wr(g + 12, (1u << 16) | 2u); wr(g + 16, 0x64000000u); wr(g + 20, 0);
    wr(g + 24, 0x04000000u); wr(g + 28, 0x64808080u); wr(g + 32, (0u << 16) | 250u); wr(g + 36, 0); wr(g + 40, 0x00100010u);
    memset(&v, 0, sizeof v);
    n = r4_hud_walk(rd, g, g + 44, R4_HUD_AUTO, visit, &v);
    CHECK(n == 1 && v.keys[0] == g + 24, "image upload skips its packet, next packet is walked");
}

/* Handler table: T at 0x8009EBCC -> rows; two rows used here. */
static void setup_scene(uint32_t phase, uint16_t major, uint16_t minor, uint32_t handler)
{
    memset(ram, 0, sizeof ram);
    wr(R4_HANDLER_TABLE_ADDR + 4u * 1u, 0x800A0000u);
    wr(R4_HANDLER_TABLE_ADDR + 4u * 2u, 0x800A0200u);
    wr(0x800A0000u + 4u * 53u, 0x8011729Cu);   /* row1[53]: overlay 660 race */
    wr(0x800A0000u + 4u * 22u, 0x8002A464u);   /* row1[22]: after-goal */
    wr(0x800A0200u + 4u * 1u, 0x80114A38u);    /* row2[1]: overlay 659 or 665 */
    wr(0x800A0000u + 4u * 5u, 0x80030000u);    /* a menu handler */
    wr(R4_RACE_PHASE_ADDR, phase);
    wrh(R4_STATE_MAJOR_ADDR, major);
    wrh(R4_STATE_MINOR_ADDR, minor);
    (void)handler;
}

static void test_scene(void)
{
    setup_scene(2, 1, 53, 0);
    wr(0x8011729Cu, 0x3C04800Fu); wr(0x801172A0u, 0x3C038010u);
    CHECK(r4_frame_handler(rd, rdh) == 0x8011729Cu, "handler lookup follows the main loop");
    CHECK(r4_in_race(rd, rdh) == 1, "overlay 660 race with its code resident");
    wr(R4_RACE_PHASE_ADDR, 4);
    CHECK(r4_in_race(rd, rdh) == 0, "phase 4 (finish/results) stays 4:3");
    wr(R4_RACE_PHASE_ADDR, 0);
    CHECK(r4_in_race(rd, rdh) == 1, "phase 0 fly-by widens");
    wr(0x801172A0u, 0x00000000u);
    CHECK(r4_in_race(rd, rdh) == 0, "overlay race handler rejected when its code is not resident");

    setup_scene(1, 2, 1, 0);
    wr(0x80114A38u, 0x27BDFFE8u); wr(0x80114A3Cu, 0x00002021u);   /* overlay 665 words */
    CHECK(r4_in_race(rd, rdh) == 0, "row2[1] with overlay 665 resident is not a race");
    wr(0x80114A38u, 0x27BDFFC8u); wr(0x80114A3Cu, 0x3C03800Fu);   /* overlay 659 */
    CHECK(r4_in_race(rd, rdh) == 1, "row2[1] with overlay 659 resident is a race");

    setup_scene(3, 1, 22, 0);
    CHECK(r4_in_race(rd, rdh) == 1, "EXE after-goal handler needs no signature");
    setup_scene(2, 1, 5, 0);
    CHECK(r4_in_race(rd, rdh) == 0, "menu handler is not a race");
    setup_scene(2, 5, 0, 0);
    CHECK(r4_frame_handler(rd, rdh) == 0 && !r4_in_race(rd, rdh), "major out of range");
    setup_scene(2, 1, 90, 0);
    CHECK(r4_frame_handler(rd, rdh) == 0, "minor out of range");
    setup_scene(2, 0, 0, 0);
    CHECK(r4_frame_handler(rd, rdh) == 0, "a null row is not followed");
}

static void test_pvs(void)
{
    uint32_t list[8] = { 10, 20, 30 };
    uint32_t count = 3;
    const uint32_t add[] = { 20, 40, 10, 50, 60, 70, 80, 90 };
    uint32_t n = r4_pvs_union(list, &count, add, 8, 6);
    CHECK(n == 3 && count == 6, "union dedups and stops at the cap");
    CHECK(list[3] == 40 && list[4] == 50 && list[5] == 60, "union keeps order");
    count = 6;
    CHECK(r4_pvs_union(list, &count, add, 8, 6) == 0, "a full list takes nothing");
    CHECK(r4_pvs_octant_reach(0) == 0, "4:3 adds no octants");
    CHECK(r4_pvs_octant_reach(53) == 1, "16:9 adds the neighbours");
    CHECK(r4_pvs_octant_reach(120) == 1, "21:9 adds the neighbours");
    CHECK(r4_pvs_octant_reach(267) == 2, "32:9 adds two octants per side");
}

int main(void)
{
    test_view();
    test_class_table();
    test_auto_thresholds();
    test_gp0_words();
    test_walker();
    test_scene();
    test_pvs();
    if (failures) {
        fprintf(stderr, "test_r4_widescreen: %d failure(s)\n", failures);
        return 1;
    }
    puts("PASS: r4 widescreen helpers");
    return 0;
}
