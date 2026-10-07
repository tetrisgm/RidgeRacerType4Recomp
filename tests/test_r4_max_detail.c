/* R4 Max Detail (r4.enhancement.max-detail) against a mock mod API.
 *
 *   - pure helpers: option parsing (a missing or unknown value keeps the
 *     most detail), the car LOD tables (the cull distance never moves), the
 *     course visibility merge shared with the widescreen plugin (r4_pvs.h:
 *     section count, wrap-around, nearest-first order, the cross product
 *     with wide octants, the 255 cap, invalid guest data, the exact
 *     widescreen octant union it replaced, and a mock table laid out with
 *     the game's 8-octant stride);
 *   - the plugin itself (src/mods/r4_max_detail_plugin.c): its hooks and
 *     plugin id, inert before activation, the draw-distance clamp switch per
 *     option, the course renderer writes, the course-list union only at
 *     Maximum, only below about 30:9 and only from the course draw paths
 *     (both return-address gates), the car table put back to Stock after
 *     a save state made with Always full, car reflections (only over the
 *     game's "off" page and only in a live race) and mirror scenery (the
 *     mirror list's count put back, only between the mirror draw's two
 *     return-address gates).
 *
 * Build/run: ctest -R r4_max_detail */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_max_detail.h"
#include "r4_pvs.h"
#include "r4_widescreen_scene.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } } while (0)

/* ---- guest memory ------------------------------------------------------ */
static uint8_t s_ram[2u * 1024u * 1024u];
static uint8_t s_spad[1024];
static int s_spad_writes, s_ram_half_writes;

static uint8_t *at(uint32_t a) {
    if ((a & 0xFFFFFC00u) == 0x1F800000u) return &s_spad[a & 0x3FFu];
    return &s_ram[a & 0x1FFFFFu];
}
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
uint16_t psx_mod_read_half(uint32_t a) { uint16_t v; memcpy(&v, at(a), 2); return v; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; memcpy(&v, at(a), 4); return v; }
void psx_mod_write_half(uint32_t a, uint16_t v) {
    if ((a & 0xFFFFFC00u) == 0x1F800000u) s_spad_writes++;
    else s_ram_half_writes++;
    memcpy(at(a), &v, 2);
}
void psx_mod_write_word(uint32_t a, uint32_t v) { memcpy(at(a), &v, 4); }
static uint32_t rd32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t rd16(uint32_t a) { return psx_mod_read_half(a); }
static void put_car_table(const int16_t rows[R4_MD_CAR_LOD_ROWS][3]);
static int car_table_is(const int16_t rows[R4_MD_CAR_LOD_ROWS][3]);

/* ---- the mod API the plugin uses ---------------------------------------- */
#define MAX_HOOKS 16
static struct { char id[32]; uint32_t addr; PSXModFunctionEntryCallback cb; } s_hooks[MAX_HOOKS];
static int s_nhooks;
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                           PSXModFunctionEntryCallback cb) {
    if (!id || !address || !cb || s_nhooks == MAX_HOOKS) return 0;
    for (int i = 0; i < s_nhooks; i++)
        if (strcmp(s_hooks[i].id, id) == 0 &&
            ((s_hooks[i].addr ^ address) & 0x1FFFFFFFu) == 0u)
            return 0;
    snprintf(s_hooks[s_nhooks].id, sizeof s_hooks[0].id, "%s", id);
    s_hooks[s_nhooks].addr = address;
    s_hooks[s_nhooks++].cb = cb;
    return 1;
}
static PSXModActivationCallback s_activate;
static char s_activation_id[32];
int psx_mod_register_activation_plugin(const char *id, PSXModActivationCallback cb) {
    snprintf(s_activation_id, sizeof s_activation_id, "%s", id);
    s_activate = cb;
    return 1;
}
static PSXModActivationCallback s_savestate;
int psx_mod_register_savestate_plugin(const char *id, PSXModActivationCallback cb) {
    (void)id;
    s_savestate = cb;
    return 1;
}
uint64_t psx_cycle_count;   /* the guest clock the frame budget reads */
static int s_ram8 = 0;
int psx_ram_8mb_active(void) { return s_ram8; }
static PSXModVBlankCallback s_vblank;
static char s_vblank_id[32];
int psx_mod_register_vblank_plugin(const char *id, PSXModVBlankCallback cb) {
    snprintf(s_vblank_id, sizeof s_vblank_id, "%s", id);
    s_vblank = cb;
    return 1;
}
#define NOPTS 6
static const char *s_options[NOPTS][2];   /* option id, value (NULL = unset) */
int psx_mod_option_value(const char *pkg, const char *feature, const char *option,
                         char *out, uint32_t out_size) {
    if (strcmp(pkg, "r4.enhancement.max-detail") != 0 || strcmp(feature, "max-detail") != 0)
        return 0;
    for (int i = 0; i < NOPTS; i++)
        if (s_options[i][0] && strcmp(s_options[i][0], option) == 0 && s_options[i][1]) {
            snprintf(out, out_size, "%s", s_options[i][1]);
            return 1;
        }
    return 0;
}
static int s_clamp = -1, s_clamp_sites = 1;
int psx_mod_set_draw_distance_clamp(int enabled) { s_clamp = enabled ? 1 : 0; return s_clamp_sites; }
int psx_mod_draw_distance_clamp_enabled(void) { return s_clamp == 1; }
static int32_t s_margin;
int32_t psx_mod_widescreen_x_margin(void) { return s_margin; }
static int s_game_started = 1;
int psx_mod_game_started(void) { return s_game_started; }

static PSXModFunctionEntryCallback hook(uint32_t addr) {
    for (int i = 0; i < s_nhooks; i++)
        if (s_hooks[i].addr == addr) return s_hooks[i].cb;
    return NULL;
}
/* The four distance options; Car reflections and Mirror scenery unset
 * (their defaults: on, stock) unless set_extra() follows. */
static void set_options(const char *draw, const char *course, const char *cars,
                        const char *split) {
    s_options[0][0] = "draw_distance"; s_options[0][1] = draw;
    s_options[1][0] = "course";        s_options[1][1] = course;
    s_options[2][0] = "cars";          s_options[2][1] = cars;
    s_options[3][0] = "split_screen";  s_options[3][1] = split;
    s_options[4][0] = "reflections";   s_options[4][1] = NULL;
    s_options[5][0] = "mirror";        s_options[5][1] = NULL;
}
static void set_extra(const char *reflections, const char *mirror) {
    s_options[4][1] = reflections;
    s_options[5][1] = mirror;
}

/* ---- a mock course: NSEC sections x 8 octants ---------------------------- */
/* Laid out with the game's own stride, not R4_PVS_COLUMNS: the lookup at
 * 0x8006F5D8 is `sll s0,s0,3; addu s0,s0,v0` (table[section * 8 + octant]).
 * The widescreen plugin once read it with 9 columns; this catches that. */
#define GAME_COLUMNS 8u
#define NSEC 10u
#define TABLE 0x80100000u
#define ENTRIES 0x80101000u
#define BLOCKS 0x80140000u
/* Section s, octant o holds blocks 100*s + 10*o + {0, 1} (two each). */
static uint32_t block_ptr(uint32_t s, uint32_t o, uint32_t k) {
    return BLOCKS + R4_PVS_BLOCK_STRIDE * (100u * s + 10u * o + k);
}
static void build_course(uint32_t per_entry) {
    memset(s_ram, 0, sizeof s_ram);
    psx_mod_write_word(R4_PVS_TABLE_PTR_ADDR, TABLE);
    psx_mod_write_word(R4_PVS_BLOCK_BASE_ADDR, BLOCKS);
    psx_mod_write_word(R4_TRACK_SEGMENT_COUNT_ADDR, NSEC * 5u);
    uint32_t e = ENTRIES;
    for (uint32_t s = 0; s < NSEC; s++)
        for (uint32_t o = 0; o < GAME_COLUMNS; o++) {
            psx_mod_write_word(TABLE + 4u * (s * GAME_COLUMNS + o), e);
            psx_mod_write_word(e, per_entry);
            for (uint32_t k = 0; k < per_entry; k++)
                psx_mod_write_half(e + 4u + 2u * k, (uint16_t)(100u * s + 10u * o + k));
            e += 4u + 2u * per_entry;
            e = (e + 3u) & ~3u;
        }
}
/* The frame list as 0x8006EB58 leaves it: section s, octant o. */
static void load_list(uint32_t s, uint32_t o, uint32_t per_entry) {
    psx_mod_write_word(R4_PVS_LIST_ADDR, per_entry);
    for (uint32_t k = 0; k < per_entry; k++)
        psx_mod_write_word(R4_PVS_LIST_ADDR + 4u + 4u * k, block_ptr(s, o, k));
}
static uint32_t list_at(uint32_t i) { return rd32(R4_PVS_LIST_ADDR + 4u + 4u * i); }
static int list_has(uint32_t p) {
    uint32_t n = rd32(R4_PVS_LIST_ADDR);
    for (uint32_t i = 0; i < n; i++) if (list_at(i) == p) return 1;
    return 0;
}

/* The widescreen plugin's octant union before it moved to r4_pvs_merge. */
static void old_ws_merge(uint32_t section, uint32_t octant, int reach) {
    uint32_t table = rd32(R4_PVS_TABLE_PTR_ADDR), blocks = rd32(R4_PVS_BLOCK_BASE_ADDR);
    uint32_t list[R4_PVS_LIST_MAX], count = rd32(R4_PVS_LIST_ADDR);
    for (uint32_t i = 0; i < count; i++) list[i] = rd32(R4_PVS_LIST_ADDR + 4u + 4u * i);
    uint32_t before = count;
    for (int d = 1; d <= reach; d++)
        for (int side = -1; side <= 1; side += 2) {
            uint32_t oct = (octant + 8u + (uint32_t)(side * d)) & 7u;
            uint32_t entry = rd32(table + 4u * (section * GAME_COLUMNS + oct));
            uint32_t n = rd32(entry), add[R4_PVS_LIST_MAX];
            for (uint32_t i = 0; i < n; i++)
                add[i] = blocks + R4_PVS_BLOCK_STRIDE * rd16(entry + 4u + 2u * i);
            (void)r4_pvs_union(list, &count, add, n, R4_PVS_LIST_MAX);
        }
    for (uint32_t i = before; i < count; i++) psx_mod_write_word(R4_PVS_LIST_ADDR + 4u + 4u * i, list[i]);
    if (count != before) psx_mod_write_word(R4_PVS_LIST_ADDR, count);
}

/* ---- pure helpers -------------------------------------------------------- */
static void test_options(void) {
    R4MdOptions o = r4_md_options(NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(o.draw == R4_MD_DRAW_MAXIMUM && o.course_full && o.cars_full && o.split_same,
          "unset options keep the most detail");
    CHECK(o.reflections && !o.mirror_full,
          "unset: car reflections on, mirror scenery stock (owner defaults)");
    o = r4_md_options("bogus", "", "x", "?", "maybe", "on");
    CHECK(o.draw == R4_MD_DRAW_MAXIMUM && o.course_full && o.cars_full && o.split_same,
          "unknown values keep the most detail");
    CHECK(o.reflections && !o.mirror_full,
          "unknown values keep the defaults: reflections on, mirror stock");
    o = r4_md_options("stock", "stock", "stock", "stock", "stock", "stock");
    CHECK(o.draw == R4_MD_DRAW_STOCK && !o.course_full && !o.cars_full && !o.split_same &&
              !o.reflections && !o.mirror_full,
          "all stock");
    CHECK(!r4_md_clamp_on(o) && !r4_md_far_xform_on(o) && !r4_md_course_hook_active(o),
          "stock does nothing");
    o = r4_md_options(NULL, NULL, NULL, NULL, "on", "full");
    CHECK(o.reflections && o.mirror_full, "reflections on, mirror full");
    o = r4_md_options("extended", "full", "full", "same", NULL, NULL);
    CHECK(o.draw == R4_MD_DRAW_EXTENDED && r4_md_clamp_on(o) && r4_md_far_xform_on(o),
          "extended: clamp and the far transform");
    o = r4_md_options("maximum", "stock", "stock", "same", NULL, NULL);
    CHECK(r4_md_clamp_on(o) && r4_md_far_xform_on(o) && r4_md_course_hook_active(o),
          "maximum: clamp and far transform; split alone runs the course hook");
    /* Maximum's levels: two sections ahead and one behind at the start, none
     * at the floor, up to twelve ahead; far cars from level 1. */
    CHECK(r4_md_level_ahead(0) == 2 && r4_md_level_behind(0) == 1 && !r4_md_level_far_cars(0),
          "level 0: two ahead, one behind, stock car distance");
    CHECK(r4_md_level_ahead(R4_MD_LEVEL_MIN) == 0 && r4_md_level_behind(R4_MD_LEVEL_MIN) == 0,
          "the floor adds no sections (what Extended draws)");
    CHECK(r4_md_level_ahead(R4_MD_LEVEL_MAX) == 32 && r4_md_level_far_cars(1),
          "the ceiling: 32 sections ahead; far cars from level 1");
    CHECK(r4_md_gov_start_level(0) == 0 && r4_md_gov_start_level(1) == 0 &&
              r4_md_gov_start_level(2) == R4_MD_LEVEL_MIN,
          "views from about 30:9 start with no sections");

    /* Car reflections: only over the game's "off" page, only in a live race,
     * only without a widescreen margin; a wide view takes back our own write. */
    o = r4_md_options(NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(r4_md_reflections_action(o, 1, 0, 0xFFFFFFFFu, 0) == R4_MD_ENV_WRITE_ON,
          "race, 4:3, page -1: write the race page");
    CHECK(r4_md_reflections_action(o, 0, 0, 0xFFFFFFFFu, 0) == 0,
          "not a race (menus set -1 too): no");
    CHECK(r4_md_reflections_action(o, 1, 0, 10u, 0) == 0 &&
              r4_md_reflections_action(o, 1, 0, 25u, 0) == 0 &&
              r4_md_reflections_action(o, 1, 0, 0u, 0) == 0 &&
              r4_md_reflections_action(o, 1, 1, 10u, 0) == 0,
          "a page the game set itself is never touched, 4:3 or wide");
    CHECK(r4_md_reflections_action(o, 1, 0, 10u, 1) == 0, "our page in 4:3 stays");
    CHECK(r4_md_reflections_action(o, 1, 1, 0xFFFFFFFFu, 0) == 0,
          "wide view: reflections stay stock (native-wide cost)");
    CHECK(r4_md_reflections_action(o, 1, 1, 10u, 1) == R4_MD_ENV_WRITE_OFF,
          "the view turned wide: our page is taken back");
    CHECK(r4_md_reflections_action(o, 0, 1, 10u, 1) == 0,
          "outside a live race our earlier write is the game's business");
    o = r4_md_options(NULL, NULL, NULL, NULL, "stock", NULL);
    CHECK(r4_md_reflections_action(o, 1, 0, 0xFFFFFFFFu, 0) == 0 &&
              r4_md_reflections_action(o, 1, 1, 10u, 1) == 0,
          "reflections stock: never");

    /* Mirror scenery: put back the built count, never more than 255. */
    o = r4_md_options(NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(r4_md_mirror_count(o, 14u, 5u) == 5u, "mirror stock: the limit stays");
    o = r4_md_options(NULL, NULL, NULL, NULL, NULL, "full");
    CHECK(r4_md_mirror_count(o, 14u, 5u) == 14u, "mirror full: the built count");
    CHECK(r4_md_mirror_count(o, 5u, 5u) == 5u && r4_md_mirror_count(o, 4u, 9u) == 9u,
          "a count the limit did not lower stays");
    CHECK(r4_md_mirror_count(o, 256u, 5u) == 5u, "an implausible count is ignored");
}

static void test_car_tables(void) {
    for (unsigned r = 0; r < R4_MD_CAR_LOD_ROWS; r++) {
        CHECK(r4_md_car_lod_full[r][2] == R4_MD_CAR_CULL,
              "every view culls at the stock forward distance, never beyond");
        if (r != 1)
            CHECK(r4_md_car_lod_stock[r][2] == r4_md_car_lod_full[r][2],
                  "the forward cull distance (T2) never changes");
        CHECK(r4_md_car_lod_full[r][0] <= r4_md_car_lod_full[r][1] &&
                  r4_md_car_lod_full[r][1] <= r4_md_car_lod_full[r][2],
              "full rows are ordered");
        CHECK(r4_md_car_lod_full[r][0] >= r4_md_car_lod_stock[r][0] &&
                  r4_md_car_lod_full[r][1] >= r4_md_car_lod_stock[r][1],
              "full never lowers a stock distance");
    }
    CHECK(r4_md_car_lod_stock[1][2] == 4096 && r4_md_car_lod_full[1][0] == 672,
          "the mirror draws cars as far as ahead but keeps its near model close");
}

static void test_pvs(void) {
    CHECK(r4_pvs_section_step(0, -1, 10) == 9 && r4_pvs_section_step(9, 2, 10) == 1 &&
              r4_pvs_section_step(4, -2, 10) == 2,
          "sections wrap around the circuit");
    build_course(2);
    CHECK(r4_pvs_section_count(rd32) == NSEC, "section count from the segment count");
    psx_mod_write_word(R4_TRACK_SEGMENT_COUNT_ADDR, 466u);
    CHECK(r4_pvs_section_count(rd32) == 94u, "a partial section counts");
    psx_mod_write_word(R4_TRACK_SEGMENT_COUNT_ADDR, 0u);
    CHECK(r4_pvs_section_count(rd32) == 0u, "no course: no sections");
    psx_mod_write_word(R4_TRACK_SEGMENT_COUNT_ADDR, NSEC * 5u);

    /* Two sections each way, own octant: nearest first, wrapping at 0. */
    load_list(0, 3, 2);
    R4PvsMerge m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 0, 3, NSEC, 2, 0);
    CHECK(m.ok && m.before == 2 && m.after == 10 && rd32(R4_PVS_LIST_ADDR) == 10,
          "four sections added");
    const uint32_t order[4] = { 9, 1, 8, 2 };
    for (int i = 0; i < 4; i++)
        CHECK(list_at(2u + 2u * (uint32_t)i) == block_ptr(order[i], 3, 0) &&
                  list_at(3u + 2u * (uint32_t)i) == block_ptr(order[i], 3, 1),
              "section order is -1, +1, -2, +2");
    /* Idempotent: a second merge adds nothing. */
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 0, 3, NSEC, 2, 0);
    CHECK(m.ok && m.before == 10 && m.after == 10, "merge is idempotent");

    /* With wide octants: the full cross product, own entry not repeated. */
    load_list(5, 0, 2);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, 1, 1);
    CHECK(m.after == 2u * 9u, "3 sections x 3 octants");
    for (uint32_t s = 4; s <= 6; s++)
        for (int d = -1; d <= 1; d++)
            CHECK(list_has(block_ptr(s, (uint32_t)(8 + d) & 7u, 0)),
                  "every section x octant pair present");
    CHECK(list_at(2) == block_ptr(5, 7, 0) && list_at(4) == block_ptr(5, 1, 0) &&
              list_at(6) == block_ptr(4, 0, 0),
          "own section's octants first, then neighbours");

    /* octant(): yaw 0xFFF wraps to octant 0, 45 degrees per octant. */
    CHECK(r4_pvs_octant(0x0000u) == 0u && r4_pvs_octant(0x00FFu) == 0u &&
              r4_pvs_octant(0x0100u) == 1u && r4_pvs_octant(0x0EFFu) == 7u &&
              r4_pvs_octant(0x0F00u) == 0u && r4_pvs_octant(0x1300u) == 2u,
          "octant from yaw as 0x8006F584 computes it");
    CHECK(R4_PVS_COLUMNS == GAME_COLUMNS, "the table has one column per octant");

    /* Without a section count only the octants are added. */
    load_list(5, 0, 2);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 5, 0, 0u, 2, 1);
    CHECK(m.after == 6, "no section count: octants only");
    load_list(5, 0, 2);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 99, 0, NSEC, 2, 0);
    CHECK(m.ok && m.after == 2, "a section past the course adds nothing");

    /* The 255 cap keeps the nearest additions. */
    build_course(60);
    load_list(5, 0, 60);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, 2, 0);
    CHECK(m.after == R4_PVS_LIST_MAX, "capped at 255");
    CHECK(list_has(block_ptr(4, 0, 59)) && list_has(block_ptr(6, 0, 59)) &&
              list_has(block_ptr(3, 0, 59)) && list_has(block_ptr(7, 0, 14)) &&
              !list_has(block_ptr(7, 0, 15)),
          "the cap cuts the last (farthest) section added");

    /* Invalid guest data writes nothing. */
    build_course(2);
    load_list(5, 0, 2);
    psx_mod_write_word(R4_PVS_LIST_ADDR, 300u);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, 2, 1);
    CHECK(!m.ok && rd32(R4_PVS_LIST_ADDR) == 300u, "an oversized list is left alone");
    load_list(5, 0, 2);
    psx_mod_write_word(R4_PVS_TABLE_PTR_ADDR, 0x1F800000u);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, 2, 1);
    CHECK(!m.ok && rd32(R4_PVS_LIST_ADDR) == 2u, "a bad table pointer is left alone");

    /* With no section reach the merge is the old widescreen union. */
    for (int reach = 0; reach <= 2; reach++)
        for (uint32_t o = 0; o < 8; o++) {
            build_course(3);
            load_list(2, o, 3);
            old_ws_merge(2, o, reach);
            uint32_t want[R4_PVS_LIST_MAX], n = rd32(R4_PVS_LIST_ADDR);
            for (uint32_t i = 0; i < n; i++) want[i] = list_at(i);
            load_list(2, o, 3);
            m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 2, o, NSEC, 0, reach);
            int same = m.after == n;
            for (uint32_t i = 0; same && i < n; i++) same = list_at(i) == want[i];
            CHECK(same, "widescreen octant union unchanged");
        }
}

/* ---- the plugin ------------------------------------------------------------ */
static CPUState s_cpu;

static void course_draw(uint32_t section, uint32_t octant, uint32_t merge_ra) {
    /* 0x8006F5AC calls octant(a0 = 0x1F800008) with s0 = section, ra 0x8006F5D8. */
    psx_mod_write_word(0x1F800008u + 0x14u, (octant << 9) - 0x100u + 0x1000u);
    s_cpu.gpr[4] = 0x1F800008u;
    s_cpu.gpr[16] = section;
    s_cpu.gpr[31] = 0x8006F5D8u;
    hook(0x8006F584u)(&s_cpu, 0x8006F584u);
    s_cpu.gpr[31] = merge_ra;
    hook(0x8007166Cu)(&s_cpu, 0x8007166Cu);
}

/* A frame handler from the race predicate (r4_widescreen_scene.h): state
 * (major 1, minor 7) -> a handler row at 0x80120000 -> `handler`, whose first
 * two words are w0, w1 (0, 0 for EXE handlers), at race phase `phase`. */
static void race_scene(uint32_t handler, uint32_t w0, uint32_t w1, uint32_t phase) {
    psx_mod_write_half(R4_STATE_MAJOR_ADDR, 1u);
    psx_mod_write_half(R4_STATE_MINOR_ADDR, 7u);
    psx_mod_write_word(R4_HANDLER_TABLE_ADDR + 4u, 0x80120000u);
    psx_mod_write_word(0x80120000u + 4u * 7u, handler);
    if (w0) {
        psx_mod_write_word(handler, w0);
        psx_mod_write_word(handler + 4u, w1);
    }
    psx_mod_write_word(R4_RACE_PHASE_ADDR, phase);
}

static void env_draw(void) {
    s_cpu.gpr[31] = 0x8002E21Cu;
    hook(0x80014A90u)(&s_cpu, 0x80014A90u);
}

/* The mirror draw 0x8006F0C8: the list holds its built count; the limit is
 * entered (ra 0x8006F0FC), the game lowers the count to `limit`, then the
 * first consumer is entered (ra 0x8006F128). */
static void mirror_draw_ra(uint32_t limit, uint32_t limit_ra, uint32_t list_ra) {
    s_cpu.gpr[31] = limit_ra;
    hook(0x80071704u)(&s_cpu, 0x80071704u);
    if (limit < rd32(R4_MD_LIST_ADDR)) psx_mod_write_word(R4_MD_LIST_ADDR, limit);
    s_cpu.gpr[31] = list_ra;
    hook(0x8006EDECu)(&s_cpu, 0x8006EDECu);
}
static void mirror_draw(uint32_t limit) {
    mirror_draw_ra(limit, 0x8006F0FCu, 0x8006F128u);
}

static void test_plugin(void) {
    CHECK(strcmp(s_activation_id, "r4.maxdetail") == 0 && s_activate, "activation registered");
    CHECK(s_nhooks == 14, "fourteen entry hooks");
    CHECK(s_savestate != NULL, "a save-state callback (the frame budget restarts)");
    for (int i = 0; i < s_nhooks; i++)
        CHECK(strcmp(s_hooks[i].id, "r4.maxdetail") == 0, "hooks belong to r4.maxdetail");
    CHECK(hook(0x80060F94u) && hook(0x8006F584u) && hook(0x8007166Cu) && hook(0x80093520u),
          "hooks: course renderer, octant, course list, DrawOTag");
    CHECK(hook(0x80014A90u) && hook(0x80071704u) && hook(0x8006EDECu),
          "hooks: env-map car draw, mirror limit, mirror list consumer");
    CHECK(hook(0x8006F160u) && hook(0x80091320u) && hook(0x8002DC00u) && hook(0x80015F60u) &&
              hook(0x8009375Cu) && hook(0x8008B330u),
          "hooks: object transform, SetTransMatrix, car draw, post-LOD, frame start, VSync");

    /* Before activation every hook is inert. */
    build_course(2);
    load_list(5, 0, 2);
    s_spad_writes = 0;
    psx_mod_write_half(R4_MD_COURSE_FAR_ADDR, 5120u);
    psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 1u);
    s_spad_writes = 0;
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(s_spad_writes == 0 && rd16(R4_MD_COURSE_FAR_ADDR) == 5120u &&
              rd32(R4_PVS_LIST_ADDR) == 2u,
          "inactive: nothing written");
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "inactive: reflections untouched");
    psx_mod_write_word(R4_MD_LIST_ADDR, 12u);
    mirror_draw(5u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 5u, "inactive: the mirror limit stands");
    load_list(5, 0, 2);
    put_car_table(r4_md_car_lod_full);
    s_ram_half_writes = 0;
    if (s_vblank) s_vblank();
    CHECK(s_vblank && s_ram_half_writes == 0 && car_table_is(r4_md_car_lod_full),
          "inactive: the VBlank callback writes nothing");
    put_car_table(r4_md_car_lod_stock);

    /* Defaults: everything on. */
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    CHECK(s_clamp == 1, "default: draw-distance clamp on");
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    CHECK(rd16(R4_MD_COURSE_FAR_ADDR) == 0x7FFFu && rd16(R4_MD_COURSE_2P_ADDR) == 0u,
          "default: full course detail and 1P subdivision");
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 10u && list_has(block_ptr(3, 0, 1)) &&
              list_has(block_ptr(7, 0, 0)),
          "default, no centreline: two sections each way");
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F090u);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 10u, "the alternate course draw path merges too");
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F140u);   /* the rear-view mirror draws without 0x8007166C */
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "another caller: no merge");
    /* octant() called from anywhere but the list lookup captures nothing. */
    load_list(5, 0, 2);
    psx_mod_write_word(0x1F800008u + 0x14u, 0xF00u);
    s_cpu.gpr[4] = 0x1F800008u;
    s_cpu.gpr[16] = 5u;
    s_cpu.gpr[31] = 0x8006F600u;
    hook(0x8006F584u)(&s_cpu, 0x8006F584u);
    s_cpu.gpr[31] = 0x8006F02Cu;
    hook(0x8007166Cu)(&s_cpu, 0x8007166Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "octant() from another caller: no capture, no merge");
    load_list(5, 0, 2);
    s_cpu.gpr[31] = 0x8006F02Cu;
    hook(0x8007166Cu)(&s_cpu, 0x8007166Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "no octant captured this frame: no merge");
    /* A live wide view adds the neighbouring sections' wide octants too. */
    s_margin = 53;
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u * 15u &&
              list_has(block_ptr(3, 7, 0)) && list_has(block_ptr(7, 1, 1)) &&
              list_has(block_ptr(5, 1, 0)),
          "wide: five sections x three octants");
    /* 29:9 (one wide octant each side): still two sections. */
    s_margin = 227;
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u * 15u, "29:9: five sections x three octants");
    /* From about 30:9 (two wide octants), 1P or split screen: no sections;
     * the widescreen plugin adds the camera section's octants. */
    for (int split = 0; split <= 1; split++) {
        s_margin = 267;
        psx_mod_write_half(R4_MD_COURSE_2P_ADDR, (uint16_t)split);
        load_list(5, 0, 2);
        course_draw(5, 0, 0x8006F02Cu);
        CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "32:9: no neighbouring sections");
        s_margin = 240;
        load_list(5, 0, 2);
        course_draw(5, 0, 0x8006F02Cu);
        CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "30:9: no neighbouring sections");
    }
    psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 0u);
    s_margin = 0;

    /* Extended: clamp, no course-list union. */
    set_options("extended", "full", "full", "same");
    s_activate();
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(s_clamp == 1 && rd32(R4_PVS_LIST_ADDR) == 2u, "extended: clamp only");

    /* Stock everything: the hooks leave the game alone. */
    set_options("stock", "stock", "stock", "stock");
    set_extra("stock", "stock");
    s_activate();
    CHECK(s_clamp == 0, "stock: clamp off");
    psx_mod_write_half(R4_MD_COURSE_FAR_ADDR, 5120u);
    psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 1u);
    s_spad_writes = 0;
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(s_spad_writes == 0 && rd16(R4_MD_COURSE_FAR_ADDR) == 5120u &&
              rd16(R4_MD_COURSE_2P_ADDR) == 1u && rd32(R4_PVS_LIST_ADDR) == 2u,
          "stock: course values and list untouched");
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    psx_mod_write_word(R4_MD_LIST_ADDR, 12u);
    mirror_draw(5u);
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu && rd32(R4_MD_LIST_ADDR) == 5u,
          "stock: no reflections in the race, the mirror limit stands");

    /* Split screen alone. */
    set_options("stock", "stock", "stock", "same");
    s_activate();
    psx_mod_write_half(R4_MD_COURSE_FAR_ADDR, 5120u);
    psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 1u);
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    CHECK(rd16(R4_MD_COURSE_FAR_ADDR) == 5120u && rd16(R4_MD_COURSE_2P_ADDR) == 0u,
          "split alone: only the 2P flag");

    /* Course detail alone. */
    set_options("stock", "full", "stock", "stock");
    s_activate();
    psx_mod_write_half(R4_MD_COURSE_FAR_ADDR, 0xFFFFu);   /* the mirror's "all far" */
    psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 1u);
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    CHECK(rd16(R4_MD_COURSE_FAR_ADDR) == 0x7FFFu && rd16(R4_MD_COURSE_2P_ADDR) == 1u,
          "course alone: only the far threshold, mirror included");

    /* A build whose game.toml lists no clamp sites still runs the rest. */
    s_clamp_sites = 0;
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    hook(0x80060F94u)(&s_cpu, 0x80060F94u);
    CHECK(rd16(R4_MD_COURSE_FAR_ADDR) == 0x7FFFu, "no clamp sites: course detail still on");
    s_clamp_sites = 1;

    /* The trace hook is inert without R4_MD_TRACE. */
    s_cpu.gpr[4] = 0x800ADCA0u + 0xB6Cu;
    hook(0x80093520u)(&s_cpu, 0x80093520u);
}

/* ---- car reflections ---------------------------------------------------------- */
static void test_reflections(void) {
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    /* The Grand Prix race handler (overlay 659), racing. */
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "default, live race: the race page");
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "and it stays (one write per race)");
    /* Time Attack (660) and VS (661) race handlers, countdown and racing. */
    race_scene(0x8011729Cu, 0x3C04800Fu, 0x3C038010u, 1u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "Time Attack: on");
    race_scene(0x80114C30u, 0x3C04800Fu, 0x3C038010u, 3u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "VS split screen: on");
    /* The game's own pages are left alone. */
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 25u);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 25u, "a page >= 0 is the game's");
    /* Not a live race: a menu handler, the finish and results (phase 4+), or
     * another overlay at the race handler's address. */
    race_scene(0x80118A68u, 0x27BDFFD0u, 0x3C048012u, 2u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "a menu that set -1 stays off");
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 4u);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "phase 4 (finish, results): untouched");
    race_scene(0x80114A38u, 0x8011555Cu, 0x8011568Cu, 2u);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu,
          "another overlay resident at the handler address: untouched");
    /* The fly-by (phase 0) is the game's: it sets 10 itself. */
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 0u);
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "phase 0 (fly-by): untouched");
    /* A wide view (native-wide renderer): stock reflections. */
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    s_margin = 53;
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "wide view: stock reflections");
    /* 4:3, written; the window turns wide mid-race: the -1 comes back; and
     * back to 4:3: on again. */
    s_margin = 0;
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "4:3 again: on");
    s_margin = 227;
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "turned wide mid-race: -1 put back");
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "and it stays off while wide");
    s_margin = 0;
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "back to 4:3: on");
    /* After our write the race ends (phase 4: the after-goal run sets 10
     * itself), then a wide view: the game's page is never taken back. */
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 4u);
    env_draw();
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    s_margin = 53;
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u,
          "a page set while out of the race is the game's: kept in a wide view");
    s_margin = 0;
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
    /* Car reflections = Stock. */
    set_extra("stock", NULL);
    s_activate();
    race_scene(0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu, 2u);
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 0xFFFFFFFFu, "reflections stock: off in races");
    /* Reflections alone (every other option Stock) still run. */
    set_options("stock", "stock", "stock", "stock");
    set_extra("on", NULL);
    s_activate();
    env_draw();
    CHECK(rd32(R4_MD_ENV_TPAGE_ADDR) == 10u, "reflections alone: on");
    psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
}

/* ---- mirror scenery ----------------------------------------------------------- */
static void test_mirror(void) {
    /* Default: Stock. */
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    psx_mod_write_word(R4_MD_LIST_ADDR, 14u);
    mirror_draw(5u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 5u, "default: the mirror keeps its stock limit");
    /* Full. */
    set_extra(NULL, "full");
    s_activate();
    psx_mod_write_word(R4_MD_LIST_ADDR, 14u);
    mirror_draw(5u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 14u, "full: the whole list");
    psx_mod_write_word(R4_MD_LIST_ADDR, 4u);
    mirror_draw(9u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 4u, "full: a list under the limit stays as built");
    /* Both return-address gates. */
    psx_mod_write_word(R4_MD_LIST_ADDR, 14u);
    mirror_draw_ra(5u, 0x80071000u, 0x8006F128u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 5u, "the limit from another caller: nothing kept");
    psx_mod_write_word(R4_MD_LIST_ADDR, 14u);
    mirror_draw_ra(5u, 0x8006F0FCu, 0x8006F048u);   /* main course draw's consumer call */
    CHECK(rd32(R4_MD_LIST_ADDR) == 5u, "the consumer from a main course draw: untouched");
    /* ... and the remembered count is used once only. */
    psx_mod_write_word(R4_MD_LIST_ADDR, 3u);
    s_cpu.gpr[31] = 0x8006F128u;
    hook(0x8006EDECu)(&s_cpu, 0x8006EDECu);
    CHECK(rd32(R4_MD_LIST_ADDR) == 3u, "no limit call this frame: nothing put back");
    /* Mirror scenery alone (every other option Stock). */
    set_options("stock", "stock", "stock", "stock");
    set_extra("stock", "full");
    s_activate();
    psx_mod_write_word(R4_MD_LIST_ADDR, 21u);
    mirror_draw(4u);
    CHECK(rd32(R4_MD_LIST_ADDR) == 21u, "mirror alone: the whole list");
}

/* ---- car table after a save state ------------------------------------------ */
static void put_car_table(const int16_t rows[R4_MD_CAR_LOD_ROWS][3]) {
    for (unsigned r = 0; r < R4_MD_CAR_LOD_ROWS; r++)
        for (unsigned i = 0; i < 3u; i++)
            psx_mod_write_half(R4_MD_CAR_LOD_TABLE + 6u * r + 2u * i, (uint16_t)rows[r][i]);
}
static int car_row_is(unsigned r, const int16_t want[3]) {
    for (unsigned i = 0; i < 3u; i++)
        if ((int16_t)rd16(R4_MD_CAR_LOD_TABLE + 6u * r + 2u * i) != want[i]) return 0;
    return 1;
}
static int car_table_is(const int16_t rows[R4_MD_CAR_LOD_ROWS][3]) {
    for (unsigned r = 0; r < R4_MD_CAR_LOD_ROWS; r++)
        if (!car_row_is(r, rows[r])) return 0;
    return 1;
}

static void test_car_restore(void) {
    CHECK(strcmp(s_vblank_id, "r4.maxdetail") == 0 && s_vblank, "VBlank callback registered");

    /* Always full: the plan's patches own the table; the callback never writes. */
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    put_car_table(r4_md_car_lod_stock);
    s_ram_half_writes = 0;
    s_vblank();
    CHECK(s_ram_half_writes == 0 && car_table_is(r4_md_car_lod_stock),
          "cars full: the callback leaves the table to the plan");

    /* A fresh session at Car detail = Stock reads but never writes. */
    set_options("maximum", "full", "stock", "stock");
    s_activate();
    s_ram_half_writes = 0;
    s_vblank();
    CHECK(s_ram_half_writes == 0, "stock table, stock cars: no writes");

    /* A save state made with Always full, loaded at Car detail = Stock. */
    put_car_table(r4_md_car_lod_full);
    s_vblank();
    CHECK(car_table_is(r4_md_car_lod_stock), "full table restored to stock");
    s_ram_half_writes = 0;
    s_vblank();
    CHECK(s_ram_half_writes == 0, "restored once, then idle");

    /* ... at Car detail = Stock, Split screen = same: rows 0-2 back to stock;
     * the 2P rows are the plan's (it re-applies the 1P row on every load). */
    set_options("maximum", "full", "stock", "same");
    s_activate();
    put_car_table(r4_md_car_lod_full);
    s_vblank();
    CHECK(car_row_is(0, r4_md_car_lod_stock[0]) && car_row_is(1, r4_md_car_lod_stock[1]) &&
              car_row_is(2, r4_md_car_lod_stock[2]) && car_row_is(3, r4_md_car_lod_full[3]) &&
              car_row_is(4, r4_md_car_lod_full[4]),
          "split same, stock cars: 1P, mirror and TV rows restored, 2P rows left to the plan");
    /* A fresh EXE before and after the plan's entry writes: never touched, so
     * the plan's guard check at the entry sees the stock bytes. */
    put_car_table(r4_md_car_lod_stock);
    s_game_started = 0;
    s_ram_half_writes = 0;
    s_vblank();
    CHECK(s_ram_half_writes == 0, "before the EXE entry: nothing");
    s_game_started = 1;
    s_vblank();
    CHECK(s_ram_half_writes == 0 && car_table_is(r4_md_car_lod_stock),
          "split same, stock cars, stock table: the plan's 2P rows are not pre-empted");
    put_car_table(r4_md_car_lod_full);
    s_game_started = 0;
    s_vblank();
    CHECK(car_table_is(r4_md_car_lod_full), "before the EXE entry: a full table stays too");
    s_game_started = 1;

    /* A state made with Split screen = same, loaded with it at Stock. */
    static const int16_t split_rows[R4_MD_CAR_LOD_ROWS][3] = {
        { 672, 3200, 8704 }, { -1, 2560, 4096 }, { 4096, 8192, 8704 },
        { 672, 3200, 8704 }, { 672, 3200, 8704 },
    };
    set_options("maximum", "full", "stock", "stock");
    s_activate();
    put_car_table(split_rows);
    s_vblank();
    CHECK(car_table_is(r4_md_car_lod_stock), "2P rows back to their stock values");

    /* A table the package never writes (another EXE) stays untouched. */
    static const int16_t other[R4_MD_CAR_LOD_ROWS][3] = {
        { 600, 3000, 8000 }, { -1, 2000, 4000 }, { 4000, 8000, 8000 },
        { 300, 3000, 8000 }, { 100, 2000, 8000 },
    };
    put_car_table(other);
    s_ram_half_writes = 0;
    s_vblank();
    CHECK(s_ram_half_writes == 0 && car_table_is(other), "an unknown table is left alone");
    put_car_table(r4_md_car_lod_stock);
}

/* ---- Maximum: direction, view cone, far objects, far cars, frame budget ----- */

/* A straight mock centreline: segment k at (1000, (nseg - k) * 300), so
 * racing (toward lower segments) runs toward +z. */
#define SEGTAB 0x80180000u
static void put_centreline(uint32_t nseg) {
    psx_mod_write_word(R4_TRACK_SEGMENT_TABLE_ADDR, SEGTAB);
    psx_mod_write_word(R4_TRACK_SEGMENT_COUNT_ADDR, nseg);
    for (uint32_t k = 0; k < nseg; k++) {
        psx_mod_write_word(SEGTAB + R4_TRACK_SEGMENT_STRIDE * k + R4_TRACK_SEGMENT_X, 1000u);
        psx_mod_write_word(SEGTAB + R4_TRACK_SEGMENT_STRIDE * k + R4_TRACK_SEGMENT_Z,
                           (nseg - k) * 300u);
    }
}
static int keep_even(void *ctx, uint32_t b) {
    (void)ctx;
    return (((b - BLOCKS) / R4_PVS_BLOCK_STRIDE) & 1u) == 0u;
}

static void test_pvs_dir(void) {
    build_course(2);
    /* Three ahead toward lower sections, one behind: own, -1, +1, -2, -3. */
    load_list(5, 0, 2);
    R4PvsMerge m = r4_pvs_merge_dir(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, -1, 3, 1, 0,
                                    NULL, NULL);
    CHECK(m.ok && m.after == 10u, "three ahead, one behind: four sections");
    CHECK(list_at(2) == block_ptr(4, 0, 0) && list_at(4) == block_ptr(6, 0, 0) &&
              list_at(6) == block_ptr(3, 0, 0) && list_at(8) == block_ptr(2, 0, 0),
          "nearest first: ahead 1, behind 1, ahead 2, ahead 3");
    load_list(5, 0, 2);
    m = r4_pvs_merge_dir(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, 1, 3, 0, 0, NULL, NULL);
    CHECK(m.after == 8u && list_at(2) == block_ptr(6, 0, 0) && list_at(6) == block_ptr(8, 0, 0) &&
              !list_has(block_ptr(4, 0, 0)),
          "facing the other way: ahead is toward higher sections");
    /* More reach than the circuit has: every section once. */
    load_list(5, 0, 2);
    m = r4_pvs_merge_dir(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, -1, 12, 1, 0, NULL, NULL);
    CHECK(m.after == 2u * NSEC, "a short circuit: each section once");
    /* The keep filter applies to other entries only. */
    load_list(5, 0, 2);
    m = r4_pvs_merge_dir(rd32, rd16, psx_mod_write_word, 5, 0, NSEC, -1, 2, 0, 1, keep_even,
                         NULL);
    CHECK(m.after == 2u + 3u * 3u - 1u && list_has(block_ptr(5, 0, 1)) &&
              !list_has(block_ptr(4, 0, 1)) && list_has(block_ptr(4, 0, 0)) &&
              !list_has(block_ptr(5, 1, 1)),
          "the filter drops added blocks, never the game's own");
    /* The symmetric form is unchanged: -1, +1, -2, +2. */
    load_list(0, 3, 2);
    m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, 0, 3, NSEC, 2, 0);
    CHECK(m.after == 10u && list_at(2) == block_ptr(9, 3, 0) && list_at(8) == block_ptr(2, 3, 0),
          "symmetric merge order");

    /* Direction from the centreline and the camera yaw. */
    put_centreline(NSEC * 5u);
    CHECK(r4_pvs_ahead_dir(rd32, 2, NSEC, 0x000u) == -1, "facing +z (the racing way): -1");
    CHECK(r4_pvs_ahead_dir(rd32, 2, NSEC, 0xF00u) == -1, "a little left of it: -1");
    CHECK(r4_pvs_ahead_dir(rd32, 2, NSEC, 0x800u) == 1, "facing back: +1");
    CHECK(r4_pvs_ahead_dir(rd32, 2, NSEC, 0x500u) == 1 && r4_pvs_ahead_dir(rd32, 2, NSEC, 0x300u) == -1,
          "the half-planes split at 90 degrees");
    psx_mod_write_word(R4_TRACK_SEGMENT_TABLE_ADDR, 0x1F800000u);
    CHECK(r4_pvs_ahead_dir(rd32, 2, NSEC, 0u) == 0, "no centreline: unknown");
    psx_mod_write_word(R4_TRACK_SEGMENT_TABLE_ADDR, SEGTAB);
    CHECK(r4_pvs_ahead_dir(rd32, 99, NSEC, 0u) == 0 && r4_pvs_ahead_dir(rd32, 2, 0, 0u) == 0,
          "a section past the course: unknown");
}

/* One course block with two polys of the first kind around world (40000,
 * 50000) (x, z; world = v + 0x8000), +/-100 in x and z. */
#define BLK 0x801C0000u
#define POLYS 0x801D0000u
static void put_block(void) {
    for (unsigned k = 0; k < R4_MD_POLY_KINDS; k++) psx_mod_write_word(r4_md_poly_kinds[k].table, 0u);
    memset(at(BLK), 0, 0x50);
    psx_mod_write_word(r4_md_poly_kinds[0].table, POLYS);
    psx_mod_write_word(BLK + r4_md_poly_kinds[0].field, (0u << 16) | 2u);
    for (uint32_t p = 0; p < 2u; p++) {
        int32_t x = 40000 - 32768, z = 50000 - 32768, d = p ? 100 : -100;
        uint32_t a = POLYS + 0x40u * p;
        uint32_t xy = (uint16_t)(x + d), zz = (uint16_t)(z + d) | ((uint32_t)(uint16_t)(z - d) << 16);
        psx_mod_write_word(a, xy);
        psx_mod_write_word(a + 4u, zz);
        psx_mod_write_word(a + 8u, (uint16_t)(x - d));
        psx_mod_write_word(a + 12u, (uint16_t)(x + d));
        psx_mod_write_word(a + 16u, zz);
        psx_mod_write_word(a + 20u, (uint16_t)(x - d));
    }
}

static void test_block_cone(void) {
    put_block();
    R4MdBounds b;
    CHECK(r4_md_block_bounds(rd32, BLK, &b) && b.cx == 40000 && b.cz == 50000 &&
              b.r >= 141 && b.r <= 145,
          "bounds: centre and half-diagonal on the ground plane");
    CHECK(b.sig == r4_md_block_sig(rd32, BLK), "the signature matches its block");
    double sl = r4_md_view_slope(0);
    CHECK(r4_md_block_visible(&b, 40000.0, 40000.0, 0.0, 1.0, sl), "10000 ahead: kept");
    CHECK(!r4_md_block_visible(&b, 40000.0, 40000.0, 0.0, -1.0, sl), "behind the camera: dropped");
    CHECK(!r4_md_block_visible(&b, 20000.0, 50000.0, 0.0, 1.0, sl), "beside the camera: dropped");
    CHECK(r4_md_block_visible(&b, 32000.0, 44000.0, 0.0, 1.0, sl), "inside the cone edge: kept");
    CHECK(!r4_md_block_visible(&b, 40000.0, 50000.0 - 30000.0, 0.0, 1.0, sl),
          "past the GTE-safe range: dropped");
    CHECK(r4_md_view_slope(240) > r4_md_view_slope(0) + 1.5, "a wide view widens the cone");
    psx_mod_write_word(BLK + r4_md_poly_kinds[0].field, 0u);
    CHECK(!r4_md_block_bounds(rd32, BLK, &b), "a block with no polygons has no bounds");
}

static void test_far_xform_math(void) {
    int32_t a[3] = { 32767, -32768, 0 }, c[3] = { 32768, 0, 0 };
    CHECK(r4_md_delta_fits(a) && !r4_md_delta_fits(c), "the 16-bit SVECTOR range");
    int16_t id[9] = { 4096, 0, 0, 0, 4096, 0, 0, 0, 4096 };
    int32_t d[3] = { 50000, -7, 40000 }, t[3];
    r4_md_apply_matrix(id, d, t);
    CHECK(t[0] == 50000 && t[1] == -7 && t[2] == 40000, "identity keeps the delta");
    int16_t rot[9] = { 0, 0, 4096, 0, 4096, 0, -4096, 0, 0 };   /* 90 degrees about y */
    r4_md_apply_matrix(rot, d, t);
    CHECK(t[0] == 40000 && t[1] == -7 && t[2] == -50000, "a rotation, exactly");
    int16_t half[9] = { 2048, 0, 0, 0, 2048, 0, 0, 0, 2048 };
    int32_t odd[3] = { -3, 3, 1 };
    r4_md_apply_matrix(half, odd, t);
    CHECK(t[0] == -2 && t[1] == 1 && t[2] == 0, "MVMVA's arithmetic shift (rounds down)");
    int32_t ok[3] = { 30000, -30000, 60000 }, wide[3] = { 30001, 0, 0 }, deep[3] = { 0, 0, 60001 };
    CHECK(r4_md_xf_safe(ok) && !r4_md_xf_safe(wide) && !r4_md_xf_safe(deep),
          "the GTE-safe translation range");
}

static void test_far_cars_pure(void) {
    R4MdOptions o = r4_md_options("extended", NULL, NULL, NULL, NULL, NULL);
    int16_t out[3];
    CHECK(r4_md_car_row_far(o, 0, r4_md_car_lod_full[0], out) && out[0] == 8704 &&
              out[1] == 8704 && out[2] == R4_MD_CAR_FAR,
          "full 1P row: only the cull distance moves (simplest model past 8704)");
    CHECK(r4_md_car_row_far(o, 3, r4_md_car_lod_stock[3], out) && out[0] == 320 &&
              out[1] == 3200 && out[2] == R4_MD_CAR_FAR,
          "stock 2P row: the same");
    CHECK(!r4_md_car_row_far(o, 1, r4_md_car_lod_full[1], out), "the mirror row is left alone");
    const int16_t odd[3] = { 600, 3000, 8000 };
    CHECK(!r4_md_car_row_far(o, 0, odd, out), "a row this package does not know is left alone");
    o = r4_md_options("stock", NULL, NULL, NULL, NULL, NULL);
    CHECK(!r4_md_car_row_far(o, 0, r4_md_car_lod_full[0], out), "stock draw distance: never");
    CHECK(R4_MD_CAR_FAR * 4 < 65535 && R4_MD_CAR_FAR < 447 * 32,
          "far cars stay inside SZ and the car renderer's ordering table");
}

static void test_governor_pure(void) {
    R4MdGovernor g;
    r4_md_gov_reset(&g, 0);
    const uint64_t B = R4_MD_FRAME_BUDGET;
    for (int i = 0; i < 19; i++) (void)r4_md_gov_update(&g, B * 7u / 10u);
    CHECK(g.level == 0, "19 calm frames: no change");
    CHECK(r4_md_gov_update(&g, B * 7u / 10u) == 1 && g.level == 1, "the 20th calm frame: up one");
    CHECK(r4_md_gov_update(&g, B * 95u / 100u) == -1 && g.level == 0 && g.hold == 30,
          "over 93 %: down one, hold");
    for (int i = 0; i < 29; i++) (void)r4_md_gov_update(&g, B * 7u / 10u);
    CHECK(g.level == 0, "held while the hold runs");
    (void)r4_md_gov_update(&g, B * 7u / 10u);
    CHECK(g.level == 1, "then up again");
    CHECK(r4_md_gov_update(&g, B) == -2 && g.level == -1 && g.hold == 60, "over 96 %: down two");
    (void)r4_md_gov_update(&g, B * 2u);
    (void)r4_md_gov_update(&g, B * 2u);
    CHECK(g.level == R4_MD_LEVEL_MIN, "never below the floor");
    r4_md_gov_reset(&g, 99);
    CHECK(g.level == R4_MD_LEVEL_MAX, "never above the ceiling");
    (void)r4_md_gov_update(&g, B * 7u / 10u);
    CHECK(g.level == R4_MD_LEVEL_MAX, "calm at the ceiling stays there");
    r4_md_gov_reset(&g, 0);
    for (int i = 0; i < 40; i++) (void)r4_md_gov_update(&g, B * 90u / 100u);
    CHECK(g.level == 0, "between the thresholds: steady");
    r4_md_gov_reset(&g, 0);
    for (int i = 0; i < 4; i++) (void)r4_md_gov_update(&g, B / 10u);
    CHECK(g.level == 0, "far under budget: 4 calm frames, no change");
    CHECK(r4_md_gov_update(&g, B / 10u) == 1, "far under budget: the 5th frame climbs");
    r4_md_gov_reset(&g, 10);
    CHECK(r4_md_gov_update_heap(&g, B / 10u, 850u) == -2,
          "heap 85 % drops two levels however idle the CPU");
    r4_md_gov_reset(&g, 10);
    for (int i = 0; i < 40; i++) (void)r4_md_gov_update_heap(&g, B / 10u, 550u);
    CHECK(g.level > 10, "heap 55 %: still climbs");
    r4_md_gov_reset(&g, 10);
    for (int i = 0; i < 40; i++) (void)r4_md_gov_update_heap(&g, B / 10u, 670u);
    CHECK(g.level == 10, "heap 67 %: steady");
}

/* One main-loop frame of the plugin: frame start, a course draw (section
 * 5, octant 0), then the VSync(1) floor wait after `busy` guest cycles. */
static void plugin_frame(uint64_t busy) {
    s_cpu.gpr[31] = R4_MD_FRAME_START_RA;
    hook(R4_MD_FRAME_START_FN)(&s_cpu, R4_MD_FRAME_START_FN);
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    psx_cycle_count += busy;
    s_cpu.gpr[31] = R4_MD_VSYNC_WAIT_RA;
    hook(R4_MD_VSYNC_FN)(&s_cpu, R4_MD_VSYNC_FN);
}

static void car_lookup(uint32_t row, uint32_t after_ra) {
    s_cpu.gpr[5] = row;
    hook(R4_MD_CAR_DRAW_FN)(&s_cpu, R4_MD_CAR_DRAW_FN);
    s_cpu.gpr[31] = after_ra;
    hook(R4_MD_CAR_AFTER_LOD_FN)(&s_cpu, R4_MD_CAR_AFTER_LOD_FN);
}

static void test_plugin_far(void) {
    /* ---- the far object transform ---- */
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    const uint32_t cam = R4_MD_CAMERA_POS_ADDR, out = 0x1F80007Cu, pos = 0x80190000u;
    const int16_t id[9] = { 4096, 0, 0, 0, 4096, 0, 0, 0, 4096 };
    for (unsigned i = 0; i < 9u; i++) psx_mod_write_half(R4_MD_CAMERA_MATRIX_ADDR + 2u * i, (uint16_t)id[i]);
    psx_mod_write_word(cam, 100000u); psx_mod_write_word(cam + 4u, 0u); psx_mod_write_word(cam + 8u, 200000u);
#define PLACE(dx, dy, dz, ra) do { \
        psx_mod_write_word(pos, 100000u + (uint32_t)(dx)); psx_mod_write_word(pos + 4u, (uint32_t)(dy)); \
        psx_mod_write_word(pos + 8u, 200000u + (uint32_t)(dz)); \
        for (unsigned i_ = 0; i_ < 3u; i_++) { psx_mod_write_word(out + 8u + 4u * i_, 0xAAAAAAAAu); \
            psx_mod_write_word(out + 0x2Cu + 4u * i_, 0xAAAAAAAAu); } \
        s_cpu.gpr[4] = out; s_cpu.gpr[5] = pos; hook(R4_MD_XF_FN)(&s_cpu, R4_MD_XF_FN); \
        s_cpu.gpr[4] = out + R4_MD_XF_MATRIX_OFF; s_cpu.gpr[31] = (ra); \
        hook(R4_MD_XF_SETTRANS_FN)(&s_cpu, R4_MD_XF_SETTRANS_FN); } while (0)
    PLACE(100, 0, 200, R4_MD_XF_SETTRANS_RA);
    CHECK(rd32(out + 0x2Cu) == 0xAAAAAAAAu && rd32(out + 8u) == 0xAAAAAAAAu,
          "a delta that fits: the game's own translation stands");
    PLACE(20000, 0, 40000, R4_MD_XF_SETTRANS_RA);
    CHECK(rd32(out + 0x2Cu) == 20000u && rd32(out + 0x34u) == 40000u && rd32(out + 8u) == 20000u &&
              rd32(out + 0x10u) == 40000u,
          "past 32767: the exact 32-bit translation");
    PLACE(20000, 0, 40000, 0x80012345u);
    CHECK(rd32(out + 0x2Cu) == 0xAAAAAAAAu, "SetTransMatrix from another caller: untouched");
    PLACE(0, 0, 70000, R4_MD_XF_SETTRANS_RA);
    CHECK((int32_t)rd32(out + 0x34u) == R4_MD_XF_BEHIND && rd32(out + 0x2Cu) == 0u,
          "past the GTE's range: moved behind the camera");
    set_options("stock", NULL, NULL, NULL);
    s_activate();
    PLACE(20000, 0, 40000, R4_MD_XF_SETTRANS_RA);
    CHECK(rd32(out + 0x2Cu) == 0xAAAAAAAAu, "draw distance stock: never");
#undef PLACE

    /* ---- far cars: raised for one lookup, put back after it ---- */
    set_options("extended", NULL, NULL, NULL);
    s_activate();
    put_car_table(r4_md_car_lod_full);
    s_cpu.gpr[5] = 0;
    hook(R4_MD_CAR_DRAW_FN)(&s_cpu, R4_MD_CAR_DRAW_FN);
    CHECK((int16_t)rd16(R4_MD_CAR_LOD_TABLE + 4u) == R4_MD_CAR_FAR &&
              (int16_t)rd16(R4_MD_CAR_LOD_TABLE) == 8704,
          "extended: the 1P cull distance is raised during the lookup");
    for (unsigned i = 0; i < 4u; i++) {
        car_lookup(i == 1u ? 1u : 0u, r4_md_car_after_lod_ra[i]);
        CHECK(car_table_is(r4_md_car_lod_full), "each of the four paths puts the row back");
    }
    s_cpu.gpr[5] = 3;
    hook(R4_MD_CAR_DRAW_FN)(&s_cpu, R4_MD_CAR_DRAW_FN);
    s_cpu.gpr[31] = 0x80012345u;
    hook(R4_MD_CAR_AFTER_LOD_FN)(&s_cpu, R4_MD_CAR_AFTER_LOD_FN);
    CHECK((int16_t)rd16(R4_MD_CAR_LOD_TABLE + 18u + 4u) == R4_MD_CAR_FAR,
          "0x80015F60 from elsewhere does not end the lookup");
    car_lookup(0, r4_md_car_after_lod_ra[3]);
    CHECK(car_table_is(r4_md_car_lod_full), "the next car's lookup restores the previous row first");
    s_cpu.gpr[5] = 1;
    hook(R4_MD_CAR_DRAW_FN)(&s_cpu, R4_MD_CAR_DRAW_FN);
    CHECK(car_table_is(r4_md_car_lod_full), "the mirror row is never raised");
    s_savestate();
    put_car_table(r4_md_car_lod_stock);

    /* ---- Maximum: the frame budget steers the reach ---- */
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    build_course(2);
    put_centreline(NSEC * 5u);
    psx_cycle_count = 1000000u;
    plugin_frame(R4_MD_FRAME_BUDGET / 2u);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 8u && list_has(block_ptr(3, 0, 0)) &&
              list_has(block_ptr(6, 0, 0)) && !list_has(block_ptr(7, 0, 0)),
          "level 0: two sections ahead (toward lower sections), one behind");
    for (int i = 0; i < 20; i++) plugin_frame(R4_MD_FRAME_BUDGET * 7u / 10u);
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 10u && list_has(block_ptr(2, 0, 0)),
          "20 calm frames: three ahead");
    put_car_table(r4_md_car_lod_full);
    s_cpu.gpr[5] = 0;
    hook(R4_MD_CAR_DRAW_FN)(&s_cpu, R4_MD_CAR_DRAW_FN);
    CHECK((int16_t)rd16(R4_MD_CAR_LOD_TABLE + 4u) == R4_MD_CAR_FAR, "level 1: far cars");
    car_lookup(0, r4_md_car_after_lod_ra[0]);
    for (int i = 0; i < 3; i++) plugin_frame(R4_MD_FRAME_BUDGET);
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 2u, "overloaded frames: down to the floor (no sections)");
    car_lookup(0, r4_md_car_after_lod_ra[0]);
    CHECK(car_table_is(r4_md_car_lod_full), "below level 1: the stock cull distance");
    /* Frames that never reach the VSync(1) wait, or VSync from elsewhere,
     * measure nothing. */
    s_cpu.gpr[31] = R4_MD_FRAME_START_RA;
    hook(R4_MD_FRAME_START_FN)(&s_cpu, R4_MD_FRAME_START_FN);
    psx_cycle_count += R4_MD_FRAME_BUDGET * 3u;
    s_cpu.gpr[31] = 0x8001E7E4u;
    hook(R4_MD_VSYNC_FN)(&s_cpu, R4_MD_VSYNC_FN);
    s_savestate();
    load_list(5, 0, 2);
    course_draw(5, 0, 0x8006F02Cu);
    CHECK(rd32(R4_PVS_LIST_ADDR) == 8u, "a save state starts again from level 0");
    put_car_table(r4_md_car_lod_stock);
}

/* 8 MB RAM: the primitive heap moves. */
static void wr32(uint32_t a, uint32_t v) { psx_mod_write_word(a, v); }
static void test_big_heap(void) {
    set_options(NULL, NULL, NULL, NULL);
    s_activate();
    wr32(0x1F800000u, 0x800ADCA0u + 0x1670u);
    s_cpu.gpr[31] = 0x8001E764u;
    hook(0x80093418u)(&s_cpu, 0x80093418u);
    CHECK(rd32(0x1F800000u) == 0x800ADCA0u + 0x1670u, "2 MB RAM: the stock heap");
    s_ram8 = 1;
    hook(0x80093418u)(&s_cpu, 0x80093418u);
    CHECK(rd32(0x1F800000u) == 0x80200000u, "8 MB RAM: buffer 0's heap moves to 0x80200000");
    wr32(0x1F800000u, 0x800ADCA0u + 0x22778u + 0x1670u);
    hook(0x80093418u)(&s_cpu, 0x80093418u);
    CHECK(rd32(0x1F800000u) == 0x80300000u, "buffer 1's to 0x80300000");
    wr32(0x1F800000u, 0x800ADCA0u + 0x1670u);
    s_cpu.gpr[31] = 0x8001E774u;
    hook(0x80093418u)(&s_cpu, 0x80093418u);
    CHECK(rd32(0x1F800000u) == 0x800ADCA0u + 0x1670u, "other ClearOTagR calls: untouched");
    s_ram8 = 0;
}

int main(void) {
    test_options();
    test_car_tables();
    test_pvs();
    test_plugin();
    test_reflections();
    test_mirror();
    test_car_restore();
    test_pvs_dir();
    test_block_cone();
    test_far_xform_math();
    test_far_cars_pure();
    test_governor_pure();
    test_plugin_far();
    test_big_heap();
    if (failures) {
        fprintf(stderr, "test_r4_max_detail: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_r4_max_detail: OK\n");
    return 0;
}
