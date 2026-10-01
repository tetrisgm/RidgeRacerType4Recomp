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
 * Both scratch values are rebuilt by the game every frame, so nothing the
 * plugin writes outlives a frame in which it did not run.
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
} R4MdOptions;

/* Manifest option values. A missing or unknown value takes the package's
 * default (the most detail), so a stale state file cannot switch detail off
 * by accident. */
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

static inline R4MdOptions r4_md_options(const char *draw, const char *course,
                                        const char *cars, const char *split)
{
    R4MdOptions o;
    o.draw = r4_md_parse_draw(draw);
    o.course_full = r4_md_parse_full(course);
    o.cars_full = r4_md_parse_full(cars);
    o.split_same = r4_md_parse_full(split);
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
