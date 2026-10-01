/* r4_max_detail.h - pure helpers for r4.enhancement.max-detail (R4 Max
 * Detail). docs/MAX_DETAIL.md has the research behind every value.
 *
 * Every frame R4 lowers detail in four places this mod can lift safely:
 *
 *   - Course "mipmapping": each course polygon past a depth threshold
 *     (scratch 0x1F80005C, rewritten every frame, -1 = everything in the
 *     rear-view mirror) is drawn flat-shaded with a half-resolution copy of
 *     its texture. Course detail = full writes 0x7FFF there at the course
 *     renderer's entry, so every polygon takes the full path.
 *   - Split-screen subdivision: the VS race sets scratch 0x1F80005E before
 *     each course draw, which selects coarser subdivision thresholds than 1P.
 *     Split screen = same as 1P writes 0 there at the same entry.
 *   - Car models: the table at 0x8009F228 gives per-view distances for three
 *     model levels and the cull distance. Car detail = full keeps the full
 *     model (3D wheels, full-resolution texture) out to the cull distance;
 *     the manifest's guarded [[patch]] entries write it. The cull distance
 *     itself (8704) is a correctness limit and never changes.
 *   - Draw distance: the course renderers drop a polygon past the end of the
 *     ordering table instead of drawing it. game.toml lists those 18 guards
 *     as [[draw_distance.clamp]] sites; the plugin switches psxrecomp's clamp
 *     on so far polygons land in the farthest slot. Maximum also adds the
 *     neighbouring track sections' visibility lists (r4_pvs.h) up to about
 *     30:9.
 *
 * Two more options are not distance reducers:
 *
 *   - Car reflections: the game draws car bodies with an environment map
 *     (the reflective look of replays) whenever the texture page at
 *     0x8009E288 is >= 0. It sets 10 at race init (the fly-by), after the
 *     goal and in every replay and attract-demo frame, and -1 at the start
 *     signal of every live race. Car reflections = on puts 10 back during
 *     the live race (on by default, owner decision 2026-10-01), in views
 *     without a widescreen margin: in psxrecomp's native-wide renderer the
 *     reflective parts cost the host many times their 4:3 cost (see
 *     r4_md_reflections_action).
 *   - Mirror scenery: the rear-view mirror draws only the first few blocks
 *     of its course list (a per-section limit, 0x80071704). Full keeps the
 *     whole list. Off by default (owner decision 2026-10-01): it costs the
 *     emulated PS1 the most time of anything here.
 *
 * Both scratch values are rebuilt by the game every frame, so nothing the
 * plugin writes outlives a frame in which it did not run. The two exceptions
 * are the car table (manifest patches) and the reflection page, which keeps
 * 10 until the game next sets it (at the next race start, or by a menu).
 */
#ifndef R4_MAX_DETAIL_H
#define R4_MAX_DETAIL_H

#include <stdint.h>
#include <string.h>

/* ---- guest layout (US) --------------------------------------------------- */
#define R4_MD_COURSE_DISPATCH_FN 0x80060F94u   /* course renderer dispatcher */
#define R4_MD_COURSE_FAR_ADDR    0x1F80005Cu   /* s16 far-path depth threshold */
#define R4_MD_COURSE_2P_ADDR     0x1F80005Eu   /* s16 nonzero: 2P subdivision */
#define R4_MD_COURSE_FAR_FULL    0x7FFFu       /* no polygon takes the far path */
#define R4_MD_CAR_LOD_TABLE      0x8009F228u   /* 5 rows x (T0, T1, T2) s16 */
#define R4_MD_CAR_LOD_ROWS       5u
#define R4_MD_CAR_CULL           8704          /* T2: never raised */

/* Car reflections. 0x80014A90 is the only reader of the page word (twice,
 * 0x80014ABC and 0x80014AE8, with no store in between); the setter
 * 0x80014A84 is called with 10 by race init (0x8003D3E0), the after-goal
 * run (0x8002A3E4) and replays/attract (0x8005EB78), with 25 by the garage,
 * and with -1 when the race overlays raise the start signal (race phase
 * 0 -> 1: 0x80114D6C in Grand Prix, 0x80117510 in Time Attack, 0x801150C0
 * in VS; Extra Trial and link battle, overlays 666/667, likewise) and by
 * the menus. */
#define R4_MD_ENV_RENDER_FN      0x80014A90u   /* car part with the env map */
#define R4_MD_ENV_TPAGE_ADDR     0x8009E288u   /* s32 env texture page, -1 = off */
#define R4_MD_ENV_TPAGE_RACE     10u           /* what race init and replays set */

/* Rear-view mirror course list. The mirror draw 0x8006F0C8 builds the list
 * (0x8006EB58), calls the limit 0x80071704, lowers the list's count word to
 * the limit (0x8006F100-0x8006F114: only the count is written; the block
 * pointers past it stay), sets up the GTE (0x8006EF88) and hands the list to
 * its first consumer 0x8006EDEC. The plugin reads the built count at the
 * limit's entry and puts it back at the consumer's entry; both gate on the
 * mirror draw's return addresses. */
#define R4_MD_MIRROR_LIMIT_FN    0x80071704u
#define R4_MD_MIRROR_LIMIT_RA    0x8006F0FCu   /* after jal 0x80071704 at 0x8006F0F4 */
#define R4_MD_MIRROR_LIST_FN     0x8006EDECu
#define R4_MD_MIRROR_LIST_RA     0x8006F128u   /* after jal 0x8006EDEC at 0x8006F120 */
#define R4_MD_LIST_ADDR          0x8010E3C0u   /* count, then block pointers */
#define R4_MD_LIST_MAX           255u          /* 0x8006EB58 caps the list here */

/* Stock car rows: 1P, rear-view mirror, TV/replay, 2P, 2P (alternate). */
static const int16_t r4_md_car_lod_stock[R4_MD_CAR_LOD_ROWS][3] = {
    { 672, 3200, 8704 },
    { -1, 2560, 4096 },
    { 4096, 8192, 8704 },
    { 320, 3200, 8704 },
    { 160, 2560, 8704 },
};
/* Car detail = full: the full model to the cull distance, and the mirror
 * keeps its near model only up close but draws cars as far as ahead. */
static const int16_t r4_md_car_lod_full[R4_MD_CAR_LOD_ROWS][3] = {
    { 8704, 8704, 8704 },
    { 672, 8704, 8704 },
    { 8704, 8704, 8704 },
    { 8704, 8704, 8704 },
    { 8704, 8704, 8704 },
};

/* ---- options --------------------------------------------------------------- */
typedef enum {
    R4_MD_DRAW_STOCK = 0,
    R4_MD_DRAW_EXTENDED = 1,   /* far polygons kept in the farthest OT slot */
    R4_MD_DRAW_MAXIMUM = 2,    /* + neighbouring sections' visibility lists */
} R4MdDrawDistance;

typedef struct {
    R4MdDrawDistance draw;
    int course_full;   /* no far path: full textures and smooth shading */
    int cars_full;     /* the manifest's patches write the rows; Stock restores */
    int split_same;    /* 2P uses 1P's course subdivision */
    int reflections;   /* env-mapped car bodies in live races too */
    int mirror_full;   /* the rear-view mirror keeps its whole course list */
} R4MdOptions;

/* Manifest option values. A missing or unknown value takes the package's
 * default, so a stale state file cannot switch a default off by accident:
 * the most detail for the four distance options and Car reflections, Stock
 * for Mirror scenery (off by default). */
static inline R4MdDrawDistance r4_md_parse_draw(const char *v)
{
    if (v && strcmp(v, "stock") == 0) return R4_MD_DRAW_STOCK;
    if (v && strcmp(v, "extended") == 0) return R4_MD_DRAW_EXTENDED;
    return R4_MD_DRAW_MAXIMUM;
}
static inline int r4_md_parse_full(const char *v)
{
    return !(v && strcmp(v, "stock") == 0);
}
static inline int r4_md_parse_mirror(const char *v)
{
    return v && strcmp(v, "full") == 0;
}

static inline R4MdOptions r4_md_options(const char *draw, const char *course,
                                        const char *cars, const char *split,
                                        const char *reflections, const char *mirror)
{
    R4MdOptions o;
    o.draw = r4_md_parse_draw(draw);
    o.course_full = r4_md_parse_full(course);
    o.cars_full = r4_md_parse_full(cars);
    o.split_same = r4_md_parse_full(split);
    o.reflections = r4_md_parse_full(reflections);
    o.mirror_full = r4_md_parse_mirror(mirror);
    return o;
}

/* psxrecomp's draw-distance clamp: on from Extended. */
static inline int r4_md_clamp_on(R4MdOptions o) { return o.draw != R4_MD_DRAW_STOCK; }

/* Neighbouring track sections added on each side of the camera's section at
 * Maximum (r4_pvs.h), given the widescreen octant reach (0 at 4:3, 1 from
 * 16:9, 2 from about 30:9). Two up to about 30:9; none from there on, so the
 * widest views draw what Extended draws. At 32:9, with full car models and
 * the Grand Prix start grid ahead (seven cars), the extra sections pushed the
 * PS1 frame past its two-VBlank budget: over 40 s two sections lost 46 game
 * frames (windows down to 21.6 game frames/s), one section 14, one section
 * with only the heading's neighbouring octants 1, none 0 like stock. Up to
 * 29:9 two sections lost none, in 1P (the grid) and 2P (the VS start).
 * Measured on Helter Skelter, docs/MAX_DETAIL.md. */
static inline int r4_md_section_reach_for(R4MdOptions o, int oct_reach)
{
    if (o.draw != R4_MD_DRAW_MAXIMUM || oct_reach >= 2) return 0;
    return 2;
}
static inline int r4_md_section_reach(R4MdOptions o)
{
    return r4_md_section_reach_for(o, 0);
}

/* Does the course renderer's entry hook have anything to write? */
static inline int r4_md_course_hook_active(R4MdOptions o)
{
    return o.course_full || o.split_same;
}

/* Car reflections, at the env-map draw's entry: what to write to the page
 * word, which holds `page`. +1 = R4_MD_ENV_TPAGE_RACE, -1 = put the game's
 * "off" (-1) back, 0 = nothing.
 *   live  - a live race: a race handler resident (the widescreen plugin's race
 *           predicate, r4_widescreen_scene.h) and the race phase 1..3
 *           (countdown and racing). The fly-by (phase 0) and everything from
 *           the finish on (4+) show the game's own reflections, the menus set
 *           -1, and only race VRAM is known to hold the environment map at
 *           page 10, so outside a live race nothing is written. The predicate
 *           knows the Grand Prix, Time Attack and VS handlers; Extra Trial and
 *           link battle (overlays 666/667) keep stock reflections, as they
 *           keep 4:3 in the widescreen plugin.
 *   wide  - psxrecomp's native-wide renderer is drawing (widescreen margin >
 *           0). The reflective parts toggle the GPU's set-mask bit (GP0 E6h)
 *           around every part, about 400 times a frame on the Grand Prix
 *           grid, which splits the renderer's textured batches, and in the
 *           native-wide renderer those extra batches are very expensive: at
 *           4K the host's scene GPU time went from 15-16 to 143-166 ms a frame,
 *           where 4:3 shows no difference (docs/MAX_DETAIL.md). The stock
 *           attract demo, which shows reflections, stalls the same way. The
 *           wide view's extra geometry plus reflections also overruns the
 *           PS1's own budget at the race start (28 game frames/s on the
 *           Helter Skelter grid at 16:9). So reflections stay stock in wide
 *           views, and turning a window wide mid-race puts back the -1 this
 *           plugin replaced.
 *   wrote - this plugin wrote the race page and the game has not set the page
 *           since (the plugin clears it when it sees -1 or leaves the race).
 * A page the game set itself (10 in the fly-by, after the goal and in replays,
 * 25 in the garage) is never touched. */
#define R4_MD_ENV_WRITE_ON   1
#define R4_MD_ENV_WRITE_OFF  (-1)
static inline int r4_md_reflections_action(R4MdOptions o, int live, int wide,
                                           uint32_t page, int wrote)
{
    if (!o.reflections || !live) return 0;
    if ((int32_t)page < 0) return wide ? 0 : R4_MD_ENV_WRITE_ON;
    return (wrote && wide) ? R4_MD_ENV_WRITE_OFF : 0;
}

/* Mirror scenery: the count word to put back at the mirror list's first
 * consumer, given the count 0x8006EB58 built (`built`, read at the limit's
 * entry) and the count the limit left (`now`). Stock, a count the limit did
 * not lower, or an implausible one leave `now`. */
static inline uint32_t r4_md_mirror_count(R4MdOptions o, uint32_t built, uint32_t now)
{
    if (!o.mirror_full || built > R4_MD_LIST_MAX || built <= now) return now;
    return built;
}

/* ---- car table after a save state --------------------------------------------
 * The car rows are written once by the manifest's [[patch]] entries, so a save
 * state carries whatever table it was made with, and loading one re-applies
 * only the current plan's writes. With Car detail = Stock the plan writes no
 * full rows, so the plugin puts the stock row back wherever it finds a row
 * this package could have left there (stock, Always full, or the 1P row that
 * Split screen = same copies into the 2P rows). Rows the current plan writes
 * itself (the 2P rows with Split screen = same) are left to the plan: the
 * plugin must not touch them before the plan's own guard check at the EXE
 * entry. A row it does not recognise is not the US table and stays untouched. */
static inline int r4_md_car_row_eq(const int16_t a[3], const int16_t b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static inline int r4_md_car_row_known(unsigned row, const int16_t cur[3])
{
    if (row >= R4_MD_CAR_LOD_ROWS) return 0;
    return r4_md_car_row_eq(cur, r4_md_car_lod_stock[row]) ||
           r4_md_car_row_eq(cur, r4_md_car_lod_full[row]) ||
           ((row == 3u || row == 4u) && r4_md_car_row_eq(cur, r4_md_car_lod_stock[0]));
}

/* Should the plugin set `row`, holding `cur`, back to r4_md_car_lod_stock? */
static inline int r4_md_car_row_restore(R4MdOptions o, unsigned row, const int16_t cur[3])
{
    if (o.cars_full || row >= R4_MD_CAR_LOD_ROWS) return 0;
    if (o.split_same && (row == 3u || row == 4u)) return 0;   /* the plan's rows */
    return r4_md_car_row_known(row, cur) && !r4_md_car_row_eq(cur, r4_md_car_lod_stock[row]);
}

#endif /* R4_MAX_DETAIL_H */
