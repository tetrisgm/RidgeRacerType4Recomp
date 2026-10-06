/* R4 Controls (r4.modern-controls) against a mock mod API: what activation
 * registers per scheme, and the Modern race mapping. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mod_plugins.h"
#include "r4_modern_controls.h"

static uint8_t s_ram[2u * 1024u * 1024u];
static void (*s_activate)(void);
static PSXModPadTransform s_xf[4];
static int s_xf_set[4];
static uint32_t s_direct;
static const char *s_scheme; /* NULL = option absent */
static int s_failures;

static uint8_t *at(uint32_t a) { return &s_ram[a & 0x1FFFFFu]; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; memcpy(&v, at(a), 4); return v; }
uint16_t psx_mod_read_half(uint32_t a) { uint16_t v; memcpy(&v, at(a), 2); return v; }

int psx_mod_register_activation_plugin(const char *id, PSXModActivationCallback fn) {
    if (!id || strcmp(id, "r4.modern-controls") != 0 || !fn) return 0;
    s_activate = fn;
    return 1;
}
int psx_mod_option_value(const char *package_id, const char *feature_id,
                         const char *option_id, char *out, uint32_t out_size) {
    if (!s_scheme || strcmp(package_id, "r4.modern-controls") != 0 ||
        strcmp(feature_id, "modern-controls") != 0 ||
        strcmp(option_id, "scheme") != 0)
        return 0;
    snprintf(out, out_size, "%s", s_scheme);
    return 1;
}
int psx_mod_set_pad_transform(uint32_t player, const PSXModPadTransform *xf) {
    if (player >= 4 || !xf) return 0;
    s_xf[player] = *xf;
    s_xf_set[player] = 1;
    return 1;
}
int psx_mod_allow_direct_shortcut(uint32_t shortcut) {
    s_direct |= 1u << shortcut;
    return 1;
}

#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL: %s\n", msg); s_failures++; } } while (0)

static void reset_mocks(void) {
    memset(s_xf, 0, sizeof s_xf);
    memset(s_xf_set, 0, sizeof s_xf_set);
    s_direct = 0;
}

/* Same scene the widescreen tests use: phase < 4 and the race handler. */
static void set_scene(uint32_t phase) {
    const uint32_t table_row = 0x8009F000u;
    const uint32_t race_handler = 0x8011729Cu;
    const uint32_t sig[] = { 0x3C04800Fu, 0x3C038010u };
    memset(at(0x800F4E1Au), 0, 2);
    memset(at(0x800F3BD6u), 0, 2);
    memcpy(at(0x800FF860u), &phase, 4);
    memcpy(at(0x8009EBCCu), &table_row, 4);
    memcpy(at(table_row), &race_handler, 4);
    memcpy(at(race_handler), sig, sizeof sig);
}

static void put16(uint32_t a, uint16_t v) { memcpy(at(a), &v, 2); }
static void put32(uint32_t a, uint32_t v) { memcpy(at(a), &v, 4); }

/* R4's default NeGcon config (byte-swapped pad word) and zero rest offsets. */
static void set_default_negcon_config(void) {
    for (uint32_t p = 0; p < 2; ++p) {
        const uint32_t cfg = 0x800F3128u + p * 48u + 16u;
        put16(cfg + 8u, 0x4000u);   /* shift up: D-pad Down */
        put16(cfg + 10u, 0x1000u);  /* shift down: D-pad Up */
        put16(cfg + 12u, 0x0010u);  /* view: Triangle (B) */
        put16(0x800AD6E0u + p * 16u + 10u, 0);
        put16(0x800AD6E0u + p * 16u + 12u, 0);
        put16(0x800AD6E0u + p * 16u + 16u, 1);   /* dead zone index */
        put16(0x800AD6E0u + p * 16u + 18u, 1);   /* range index */
    }
    {
        static const uint16_t dz[4] = { 0, 6, 10, 14 };
        static const uint16_t range[4] = { 25, 38, 75, 113 };
        for (uint32_t i = 0; i < 4; ++i) {
            put16(0x800A0324u + i * 4u, dz[i]);
            put16(0x800A0170u + i * 2u, range[i]);
        }
    }
    put32(0x800F4F6Cu, 0);
}

static PSXModPadFrame frame(uint32_t buttons, uint32_t type, uint32_t lx,
                            uint32_t flags, uint32_t lt, uint32_t rt) {
    PSXModPadFrame f;
    memset(&f, 0, sizeof f);
    f.struct_size = sizeof f;
    f.buttons = buttons;
    f.type = type;
    f.lx = lx; f.ly = 0x80; f.rx = 0x37; f.ry = 0xD2;
    f.host_flags = flags;
    f.host_lt = lt; f.host_rt = rt;
    return f;
}

static uint32_t press(uint32_t mask) { return 0xFFFFu & ~mask; }

/* Runs the registered transform the way mod_pad_transform_run does. */
static int run(const PSXModPadFrame *f, PSXModPadOutput *o) {
    memset(o, 0, sizeof *o);
    o->struct_size = sizeof *o;
    o->buttons = f->buttons; o->type = f->type;
    o->lx = f->lx; o->ly = f->ly; o->rx = f->rx; o->ry = f->ry;
    return s_xf[0].transform(f, o);
}

static void test_activation(void) {
    CHECK(s_activate != NULL, "activation plugin registered");
    if (!s_activate) return;

    reset_mocks(); s_scheme = NULL; s_activate();
    CHECK(s_xf_set[0] && s_xf_set[1], "default scheme registers P1/P2 transforms");
    CHECK(s_direct == (1u << PSX_MOD_SHORTCUT_REWIND), "default allows direct Rewind only");

    reset_mocks(); s_scheme = "modern"; s_activate();
    CHECK(s_xf_set[0] && s_xf_set[1] && !s_xf_set[2], "modern registers two players");
    CHECK(s_xf[0].struct_size == sizeof(PSXModPadTransform), "struct_size");
    CHECK(s_xf[0].allowed_types & PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_NEGCON), "NeGcon allowed");
    CHECK(s_xf[0].allowed_types & PSX_MOD_PAD_TYPE_BIT(s_xf[0].initial_type),
          "initial type allowed");
    CHECK(!(s_xf[0].allowed_types & PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_JOGCON)),
          "JogCon is never produced");
    CHECK(s_direct & (1u << PSX_MOD_SHORTCUT_REWIND), "modern allows direct Rewind");

    reset_mocks(); s_scheme = "classic"; s_activate();
    CHECK(!s_xf_set[0] && !s_xf_set[1] && !s_direct, "classic registers nothing");
}

static void test_mapping(void) {
    const uint32_t all = R4_MC_HOST_TRIGGERS;
    PSXModPadFrame f;
    PSXModPadOutput o;
    reset_mocks(); s_scheme = "modern"; s_activate();
    if (!s_xf[0].transform) { CHECK(0, "transform registered"); return; }
    set_default_negcon_config();

    /* Outside a race: pass through. */
    set_scene(4u);
    f = frame(press(R4_MC_PAD_CIRCLE), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 255);
    CHECK(run(&f, &o) == 0, "menus pass through");

    set_scene(2u);
    f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0x23, all, 64, 128);
    CHECK(run(&f, &o) == 1 && o.type == PSX_MOD_PAD_NEGCON, "race presents NeGcon");
    CHECK(o.lx == 128u - (6u + (93u * 38u + 126u) / 127u) && o.negcon_i == 53 &&
          o.negcon_ii == 27 && o.negcon_l == 0,
          "twist from lx, I = RT and II = LT scaled to 0..106, L = 0");
    {
        static const struct { uint32_t lx, twist; } sweep[] = {
            { 0x80, 0x80 }, { 0x81, 0x80 + 7 }, { 0x7F, 0x80 - 7 },
            { 0xFF, 0x80 + 44 }, { 0x00, 0x80 - 44 }, { 0x01, 0x80 - 44 },
            { 0xC0, 0x80 + 6 + 20 },
        };
        for (size_t i = 0; i < sizeof sweep / sizeof sweep[0]; ++i) {
            f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, sweep[i].lx, all, 0, 0);
            run(&f, &o);
            CHECK(o.lx == sweep[i].twist, "stick spans R4's twist dead zone + range");
        }
        put16(0x800AD6E0u + 18u, 3);   /* widest range setting: 113 */
        f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0xFF, all, 0, 0);
        run(&f, &o);
        CHECK(o.lx == 0x80 + 6 + 113, "R4's twist range setting followed");
        put16(0x800AD6E0u + 18u, 1);
    }
    CHECK(o.buttons == 0xFFFFu, "no buttons at rest");

    f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0x80, all, 255, 255);
    run(&f, &o);
    CHECK(o.negcon_i == 106 && o.negcon_ii == 106, "full triggers = R4's full pressure");
    f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.negcon_i == 0 && o.negcon_ii == 0, "released triggers = zero pressure");
    put16(0x800AD6E0u + 10u, 20); put16(0x800AD6E0u + 12u, 250);
    f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 255);
    run(&f, &o);
    CHECK(o.negcon_i == 126 && o.negcon_ii == 250, "rest offsets added");
    f = frame(0xFFFFu, PSX_MOD_PAD_DUALSHOCK, 0x80, all, 255, 0);
    run(&f, &o);
    CHECK(o.negcon_i == 20 && o.negcon_ii == 255, "pressure clamped to 255");
    put16(0x800AD6E0u + 10u, 0); put16(0x800AD6E0u + 12u, 0);

    f = frame(press(R4_MC_PAD_SQUARE), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == press(R4_MC_PAD_UP), "Square -> NeGcon shift down (Up)");
    f = frame(press(R4_MC_PAD_CIRCLE), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == press(R4_MC_PAD_DOWN), "Circle -> NeGcon shift up (Down)");
    f = frame(press(R4_MC_PAD_R1), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == press(R4_MC_PAD_TRIANGLE), "R1 -> NeGcon view (B)");
    f = frame(press(R4_MC_PAD_CROSS | R4_MC_PAD_L1 | R4_MC_PAD_L2 | R4_MC_PAD_R2 |
                    R4_MC_PAD_TRIANGLE | R4_MC_PAD_UP | R4_MC_PAD_DOWN |
                    R4_MC_PAD_LEFT | R4_MC_PAD_RIGHT),
              PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == 0xFFFFu, "Cross/L1/L2/R2/Triangle/D-pad reach nothing");
    f = frame(press(0x0008u | 0x0001u), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == press(0x0008u | 0x0001u), "Start and Select pass");

    /* An in-game remap of the NeGcon buttons is followed. */
    put16(0x800F3128u + 16u + 8u, 0x0008u);   /* shift up on R (SIO 0x0800) */
    f = frame(press(R4_MC_PAD_CIRCLE), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.buttons == press(R4_MC_PAD_R1), "remapped shift up followed");
    put16(0x800F3128u + 16u + 8u, 0);
    run(&f, &o);
    CHECK(o.buttons == press(R4_MC_PAD_DOWN), "empty config falls back to default");
    set_default_negcon_config();

    /* Digital-mode pad: full lock from the D-pad. */
    f = frame(press(R4_MC_PAD_LEFT), PSX_MOD_PAD_DIGITAL, 0x80, all, 0, 255);
    run(&f, &o);
    CHECK(o.type == PSX_MOD_PAD_NEGCON && o.lx == 0x00 && o.negcon_i == 106 &&
          o.buttons == 0xFFFFu, "digital left = full left twist");
    f = frame(press(R4_MC_PAD_RIGHT), PSX_MOD_PAD_DIGITAL, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.lx == 0xFF, "digital right = full right twist");
    f = frame(0xFFFFu, PSX_MOD_PAD_DIGITAL, 0x80, all, 0, 0);
    run(&f, &o);
    CHECK(o.lx == 0x80, "digital neutral = centred twist");

    /* Keyboard, one trigger missing, wheels: stock. */
    f = frame(press(R4_MC_PAD_CROSS), PSX_MOD_PAD_DUALSHOCK, 0x80, 0, 0, 0);
    CHECK(run(&f, &o) == 0, "keyboard passes through");
    f = frame(press(R4_MC_PAD_CROSS), PSX_MOD_PAD_DUALSHOCK, 0x80,
              PSX_MOD_PAD_HOST_GAMEPAD | PSX_MOD_PAD_HOST_RT, 0, 0);
    CHECK(run(&f, &o) == 0, "pad without LT passes through");
    f = frame(press(R4_MC_PAD_CROSS), PSX_MOD_PAD_JOGCON, 0x80, all, 0, 0);
    CHECK(run(&f, &o) == 0, "wheel (JogCon) passes through");

    /* Pause menu and attract demo: stock. */
    put32(0x800F4F6Cu, 1);
    f = frame(press(R4_MC_PAD_CIRCLE), PSX_MOD_PAD_DUALSHOCK, 0x80, all, 0, 0);
    CHECK(run(&f, &o) == 0, "pause menu passes through");
    put32(0x800F4F6Cu, 0);
    {
        const uint32_t attract = 0x8005E118u;
        memcpy(at(0x8009F000u), &attract, 4);
        CHECK(run(&f, &o) == 0, "attract demo passes through");
        set_scene(2u);
        CHECK(run(&f, &o) == 1, "race again after attract");
    }
    f.player = 2;
    CHECK(run(&f, &o) == 0, "players past P2 pass through");

    CHECK(r4_modern_controls_scheme_is_classic("classic") &&
          !r4_modern_controls_scheme_is_classic("modern") &&
          !r4_modern_controls_scheme_is_classic(NULL), "scheme parsing");
}

int main(void) {
    test_activation();
    test_mapping();
    if (s_failures) {
        fprintf(stderr, "%d failure(s)\n", s_failures);
        return 1;
    }
    puts("R4 controls tests passed");
    return 0;
}
