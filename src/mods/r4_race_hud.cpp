#include "r4_race_hud.h"
/* Modern race HUD for R4.
 *
 * Drawn by the host as vectors (Dear ImGui, already linked for the in-game
 * menu) at the window resolution, on the engine's HUD layer
 * (psx_host_set_hud_draw_cb): under the in-game menu, on every presented
 * frame (generated frames included). Values are read from guest RAM. While
 * the panels draw, the stock elements they replace are skipped (see "stock
 * HUD" below). PSX_R4_HUD=0 turns the panels off.
 *
 * The same layer hosts the car effects (r4_car_effects.cpp), which draw under
 * the panels; each part follows its own plugin.
 *
 * Map: the track's centre line read from RAM at the start (every track);
 * failing that, traces in <exe>/assets/r4_hud/<track>.txt ("x z" per point,
 * one CPU car over a lap); at the start the trace nearest the car is used. */
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "r4_car_effects.h"

extern "C" {
#include "cpu_state.h"
#include "host_overlay.h"
#include "mod_plugins.h"
}
#include <SDL3/SDL.h>

#if defined(R4_REGION_PAL)
/* Message language of the PAL disc (0 English, 1 French, 2 German, 3 Spanish,
 * 4 Italian); the USA disc is English only. */
extern "C" int r4_pal_message_language(void);
#endif

#define PLUGIN_ID "r4.race-hud"

namespace {

std::atomic<bool> s_enabled{false};
int s_needle_hook;
/* The last full-screen HUD frame drew the position panel (stock rank hidden). */
std::atomic<bool> s_rank_shown{false};

constexpr uint32_t CAR = 0x800AC0B0u;          /* player 1's car */
constexpr uint32_t RACE_WORD = 0x800AC794u;    /* 0x180 in a race */
constexpr uint32_t CAR_BYTES = 0x320u;         /* player 2's car = CAR + 0x320 */
constexpr uint32_t LAP_COUNT = 0x800F3BE0u;    /* s32, laps in the race (0x8002BEA4) */
constexpr uint32_t RECORD = 0x800F4E24u;       /* s32, track record in ms */
constexpr uint32_t LAP_TIMES = 0x800F4E30u;    /* s32[], each lap's time in ms (-1: not yet) */
constexpr uint32_t VIEW_SETUP = 0x8010C0B8u;   /* 0x800AE810 in split screen (one player: 0x800ADD10) */
constexpr uint32_t VIEW_SETUP_2P = 0x800AE810u;
/* Current frame handler (r4_widescreen_scene.h) */
constexpr uint32_t STATE_MAJOR = 0x800F4E1Au, STATE_MINOR = 0x800F3BD6u, HANDLER_TABLE = 0x8009EBCCu;
constexpr uint32_t DEMO_HANDLER = 0x8005E118u;
constexpr uint32_t FRAMES = 0x800AC064u;       /* game frames (30/s) */
constexpr uint32_t CAR_WRONG_WAY = 0x1EAu;     /* s16, 1 when going the wrong way (0x80022804 via 0x80022B20) */
constexpr uint32_t TIME_ATTACK_HANDLER = 0x8011729Cu;  /* race handler of overlay 660 (Time Attack) */
constexpr uint32_t CAR_COUNT = 0x800AC754u;    /* s16 */
/* Car +0x1EE (s16): rank, 0 = first (overlays 659 and 661 -> 0x80021A20). */
constexpr uint32_t CAR_RANK = 0x1EEu;
/* Section split (0x8003C4B4): shown while the countdown 0x8010C244 (s32, 60
 * game frames at the split) is positive, when the mode 0x800F4EF4 is 0 and
 * the phase 0x800FF860 is 1..3. Split = player time 0x8010C250[s] - reference
 * 0x8010C23C, s = section just passed (s16 0x8010C234), in ms (measured in
 * game); negative: faster. With no section times saved the reference is the
 * whole-lap record: a split of tens of seconds, badge hidden. */
constexpr uint32_t SEC_MODE = 0x800F4EF4u, SEC_PHASE = 0x800FF860u, SEC_COUNTDOWN = 0x8010C244u,
                   SEC_PASSED = 0x8010C234u, SEC_TIME = 0x8010C23Cu, SEC_REF = 0x8010C250u,
                   SEC_CURRENT = 0x8010C25Cu;

/* HUD strings per message language, all caps; the gear's "N" and the track
 * names stay as they are. */
struct Strings {
    const char *lap, *best_lap, *record, *position, *section, *wrong_way, *final_lap, *new_best_lap,
        *kmh, *player1, *player2;
};
const Strings k_strings[] = {
    /* English: the USA disc's speedometer reads mph (speed_shown) */
    {"LAP", "BEST LAP", "RECORD", "POSITION", "SECTION", "WRONG WAY", "FINAL LAP", "NEW BEST LAP",
#if defined(R4_REGION_PAL)
     "KM/H",
#else
     "MPH",
#endif
     "PLAYER 1", "PLAYER 2"},
    {"TOUR", "MEILLEUR TOUR", "RECORD", "POSITION", "SECTION", "CONTRESENS", "DERNIER TOUR",
     "MEILLEUR TOUR", "KM/H", "JOUEUR 1", "JOUEUR 2"},
    {"RUNDE", "BESTE RUNDE", "REKORD", "POSITION", "SEKTOR", "FALSCHE RICHTUNG", "LETZTE RUNDE",
     "NEUE BESTZEIT", "KM/H", "SPIELER 1", "SPIELER 2"},
    {"VUELTA", "MEJOR VUELTA", "R\xC3\x89" "CORD", "POSICI\xC3\x93N", "SECTOR", "SENTIDO CONTRARIO",
     "\xC3\x9ALTIMA VUELTA", "MEJOR VUELTA", "KM/H", "JUGADOR 1", "JUGADOR 2"},
    {"GIRO", "MIGLIOR GIRO", "RECORD", "POSIZIONE", "SETTORE", "CONTROMANO", "ULTIMO GIRO",
     "MIGLIOR GIRO", "KM/H", "GIOCATORE 1", "GIOCATORE 2"},
};

const Strings &text_table() {
    int language = 0;
#if defined(R4_REGION_PAL)
    language = r4_pal_message_language();
#endif
    if (language < 0 || language >= (int)(sizeof k_strings / sizeof k_strings[0])) language = 0;
    return k_strings[language];
}

struct TrackMap {
    std::string name;
    std::vector<ImVec2> pts;
    float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
};

struct Hud {
    ImGuiContext *ctx = nullptr;
    bool gl = false;
    ImFont *font = nullptr;
    std::vector<TrackMap> maps;
    int map = -1;
    int ram_map = -1;          /* maps entry built from RAM (race_map) */
    uint32_t prev_handler = 0; /* demo -> race, reload: the map is rebuilt */
    int check = 0;
    bool was_in_race = false;
    /* messages: start game frame (0x800AC064), -1: none */
    int prev_lap = 0;
    int32_t msg_final = -1, msg_best = -1, wrong_way_since = -1;
} g;

int16_t r16(uint32_t a) { return (int16_t)psx_mod_read_half(a); }
int32_t r32(uint32_t a) { return (int32_t)psx_mod_read_word(a); }

/* The figure the stock speedometer shows. The race handler (overlay 660,
 * 0x80115EE0..F14) turns the car's speed +0x1D8 into km/h (x 160 x 0.8767 /
 * 1024) and passes it to 0x80021614, which on the USA disc shows mph
 * (km/h x 5 / 8, rounded towards zero). +0x296 is neither: 150 where the
 * stock gauge read 106 mph (170 km/h). */
int speed_shown(uint32_t car) {
    const int32_t a0 = (int32_t)r16(car + 0x1D8u) * 160;
    const int32_t hi = (int32_t)(((int64_t)a0 * (int32_t)0xE070381Du) >> 32);
    const int32_t kmh = ((hi + a0) >> 10) - (a0 >> 31);
#if defined(R4_REGION_PAL)
    return kmh;
#else
    return (kmh * 5 + (kmh < 0 ? 7 : 0)) >> 3;
#endif
}

uint32_t frame_handler() {
    const uint32_t major = psx_mod_read_half(STATE_MAJOR), minor = psx_mod_read_half(STATE_MINOR);
    if (major > 4u || minor > 89u) return 0;
    const uint32_t row = psx_mod_read_word(HANDLER_TABLE + 4u * major);
    if ((row & 0xFFE00003u) != 0x80000000u) return 0;
    return psx_mod_read_word(row + 4u * minor);
}

constexpr uint32_t AFTER_FINISH_HANDLER = 0x8002A464u;   /* filmed victory lap */

/* In a race, attract demo and victory lap excluded (the game shows its own
 * presentation there). */
bool in_race() {
    const uint32_t f = frame_handler();
    return psx_mod_read_half(RACE_WORD) == 0x180u && f != DEMO_HANDLER && f != AFTER_FINISH_HANDLER;
}
bool split_screen() { return psx_mod_read_word(VIEW_SETUP) == VIEW_SETUP_2P; }

/* ms -> "m:ss.mmm"; negative: no time yet */
std::string lap_time(int32_t ms) {
    if (ms < 0) return "-:--.---";
    char b[32];
    std::snprintf(b, sizeof b, "%d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
    return b;
}

void load_maps() {
    const char *base = SDL_GetBasePath();
    const std::string dir = std::string(base ? base : "") + "assets/r4_hud/";
    for (const char *name : {"helter_skelter", "wonderhill", "edge_of_the_earth", "out_of_blue",
                             "phantomile", "brightest_nite", "heaven_and_hell", "shooting_hoops"}) {
        FILE *f = std::fopen((dir + name + ".txt").c_str(), "r");
        if (!f) continue;
        TrackMap c;
        c.name = name;
        float x, z;
        while (std::fscanf(f, "%f %f", &x, &z) == 2) c.pts.push_back(ImVec2(x, z));
        std::fclose(f);
        if (c.pts.size() < 10) continue;
        c.x0 = c.x1 = c.pts[0].x; c.z0 = c.z1 = c.pts[0].y;
        for (const ImVec2 &p : c.pts) {
            c.x0 = std::fmin(c.x0, p.x); c.x1 = std::fmax(c.x1, p.x);
            c.z0 = std::fmin(c.z0, p.y); c.z1 = std::fmax(c.z1, p.y);
        }
        g.maps.push_back(std::move(c));
    }
}

bool g_maps_loaded = false;
void load_maps_once() {
    if (!g_maps_loaded) { g_maps_loaded = true; load_maps(); }
}

int nearest_map(float x, float z) {
    int best = -1;
    float dmin = 2500.f * 2500.f;   /* beyond this no trace passes there */
    for (size_t i = 0; i < g.maps.size(); ++i) {
        if ((int)i == g.ram_map) continue;
        for (const ImVec2 &p : g.maps[i].pts) {
            const float d = (p.x - x) * (p.x - x) + (p.y - z) * (p.y - z);
            if (d < dmin) { dmin = d; best = (int)i; }
        }
    }
    return best;
}

/* Map of the current race from the track's centre line in RAM (r4_pvs.h):
 * segment table *0x800AC05C (0x3C-byte records, x and z at +8 and +0xC),
 * count 0x800AC084. Measured in game: x and z there are in the frame of the
 * car's +0x20/+0x28 (world >> 2), like the map. The name comes from the
 * nearest recorded trace (assets/r4_hud), if there is one. */
constexpr uint32_t SEGMENTS = 0x800AC05Cu, SEGMENT_COUNT = 0x800AC084u, SEGMENT_BYTES = 0x3Cu;

int race_map(float x, float z) {
    const int nearest = nearest_map(x, z);
    const uint32_t tab = (uint32_t)r32(SEGMENTS);
    const int32_t n = r32(SEGMENT_COUNT);
    if ((tab & 0xFFE00003u) != 0x80000000u || n < 20 || n > 8000) return nearest;
    TrackMap c;
    c.name = nearest >= 0 ? g.maps[nearest].name : std::string();
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t r = tab + SEGMENT_BYTES * (uint32_t)i;
        c.pts.push_back(ImVec2((float)r32(r + 8u), (float)r32(r + 0xCu)));
    }
    c.x0 = c.x1 = c.pts[0].x; c.z0 = c.z1 = c.pts[0].y;
    for (const ImVec2 &p : c.pts) {
        c.x0 = std::fmin(c.x0, p.x); c.x1 = std::fmax(c.x1, p.x);
        c.z0 = std::fmin(c.z0, p.y); c.z1 = std::fmax(c.z1, p.y);
    }
    /* the car must be on this trace (otherwise an unexpected frame: keep the recorded trace) */
    float dmin = 1e30f;
    for (const ImVec2 &p : c.pts) dmin = std::fmin(dmin, (p.x - x) * (p.x - x) + (p.y - z) * (p.y - z));
    if (dmin > 1000.f * 1000.f || c.x1 - c.x0 < 100.f || c.z1 - c.z0 < 100.f) return nearest;
    if (g.ram_map >= 0) g.maps[g.ram_map] = std::move(c);
    else { g.maps.push_back(std::move(c)); g.ram_map = (int)g.maps.size() - 1; }
    return g.ram_map;
}

bool file_exists(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

/* ImGui 1.92: one font, rendered at any size (dynamic atlas). Bahnschrift
 * (Windows, DIN style) when the system has it, else the launcher's Lato
 * staged beside the executable (<exe>/assets/fonts), else ImGui's own. */
void load_fonts() {
    ImGuiIO &io = ImGui::GetIO();
    ImFont *f = nullptr;
    const char *base = SDL_GetBasePath();
    const std::string lato = std::string(base ? base : "") + "assets/fonts/LatoLatin-Bold.ttf";
    for (const std::string &path : {std::string("C:/Windows/Fonts/bahnschrift.ttf"), lato})
        if (!f && file_exists(path)) f = io.Fonts->AddFontFromFileTTF(path.c_str(), 32.f);
    if (!f) f = io.Fonts->AddFontDefault();
    g.font = f;
}

float g_s = 1.f;   /* scale: screen height / 720 */

ImU32 rgba(int r, int gr, int b, float a) { return IM_COL32(r, gr, b, (int)(a * 255)); }

enum Size { SMALL, MEDIUM, DIGITS, LARGE };
constexpr float k_text_px[] = {15.f, 21.f, 46.f, 72.f};

float text_width(Size t_, const char *t) {
    return g.font->CalcTextSizeA(k_text_px[t_] * g_s, 1e9f, 0, t).x;
}

void text(ImDrawList *dl, Size t_, ImVec2 p, ImU32 c, const std::string &t, float align = 0) {
    const float sz_px = k_text_px[t_] * g_s;
    const ImVec2 sz = g.font->CalcTextSizeA(sz_px, 1e9f, 0, t.c_str());
    dl->AddText(g.font, sz_px, ImVec2(p.x - sz.x * align, p.y), c, t.c_str());
}

void panel(ImDrawList *dl, ImVec2 a, ImVec2 b, float s, float r = 10) {
    dl->AddRectFilled(a, b, rgba(8, 12, 18, 0.55f), r * s);
    dl->AddRect(a, b, rgba(255, 255, 255, 0.14f), r * s, 0, 1.f * s);
}

void draw_hud(int W, int H) {
    (void)H;
    const Strings &tx = text_table();
    const float s = g_s;
    const ImU32 white = rgba(255, 255, 255, 1), grey = rgba(255, 255, 255, 0.72f),
                pale = rgba(255, 255, 255, 0.5f), amber = IM_COL32(255, 176, 32, 255),
                red = IM_COL32(255, 59, 48, 255);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    auto P = [&](float x, float y) { return ImVec2(x * s, y * s); };
    const float xr = W - 1280.f * s;   /* right anchor (screens wider than 16:9) */
    auto R = [&](float x, float y) { return ImVec2(xr + x * s, y * s); };
    /* centre anchor: the window centre at any aspect (16:9 places it at 640) */
    const float xc = W * 0.5f - 640.f * s;
    auto C = [&](float x, float y) { return ImVec2(xc + x * s, y * s); };

    const int kmh = speed_shown(CAR), rpm = r32(CAR + 0x27Cu), gear = r16(CAR + 0x27Au);
    const int laps = std::max(1, std::min(8, (int)r32(LAP_COUNT)));
    const int lap = std::max(1, std::min(laps, (int)r16(CAR + 0x2AAu)));
    int32_t times[8], best = -1;
    for (int i = 0; i < laps; ++i) {
        times[i] = r32(LAP_TIMES + 4u * i);
        if (i + 1 < lap && times[i] > 0 && (best < 0 || times[i] < best)) best = times[i];
    }
    const int32_t clock = times[lap - 1];
    const int32_t record = r32(RECORD);
    const int cars = r16(CAR_COUNT);
    const bool show_rank = cars > 1 && cars <= 8 && frame_handler() != TIME_ATTACK_HANDLER;
    s_rank_shown.store(show_rank, std::memory_order_relaxed);

    /* Top left: lap and lap clock */
    panel(dl, P(28, 24), P(288, 150), s);
    text(dl, SMALL, P(46, 36), grey, tx.lap);
    text(dl, MEDIUM, P(270, 33), white, std::to_string(lap) + " / " + std::to_string(laps), 1);
    text(dl, DIGITS, P(44, 58), white, lap_time(clock < 0 ? 0 : clock));
    dl->AddLine(P(46, 116), P(270, 116), rgba(255, 255, 255, 0.14f), 1.f * s);
    text(dl, SMALL, P(46, 124), grey, tx.best_lap);
    text(dl, MEDIUM, P(270, 121), best < 0 ? pale : white, lap_time(best), 1);

    /* Top right: record and lap times */
    panel(dl, R(1024, 24), R(1252, 78 + laps * 19.f), s);
    text(dl, SMALL, R(1042, 36), grey, tx.record);
    text(dl, MEDIUM, R(1234, 33), amber, lap_time(record > 0 ? record : -1), 1);
    dl->AddLine(R(1042, 64), R(1234, 64), rgba(255, 255, 255, 0.14f), 1.f * s);
    for (int i = 0; i < laps; ++i) {
        const bool done = i + 1 < lap, is_best = done && times[i] == best;
        text(dl, SMALL, R(1042, 72 + i * 19.f), pale, std::string(tx.lap) + " " + std::to_string(i + 1));
        text(dl, SMALL, R(1234, 72 + i * 19.f), is_best ? amber : (done || i + 1 == lap) ? white : pale,
             done || i + 1 == lap ? lap_time(times[i] < 0 ? 0 : times[i]) : std::string("-:--.---"), 1);
    }

    /* Top centre: rank (not in Time Attack) */
    if (show_rank) {
        const int rank = r16(CAR + CAR_RANK) + 1;
        panel(dl, C(640 - 70, 24), C(640 + 70, 84), s);
        text(dl, SMALL, C(640, 28), grey, tx.position, 0.5f);
        text(dl, MEDIUM, C(640, 46), rank == 1 ? amber : white,
             std::to_string(rank) + " / " + std::to_string(cars), 0.5f);
    }

    /* Top centre: split when passing a section (1 s of game time), green when faster */
    const int32_t countdown = r32(SEC_COUNTDOWN);
    const uint32_t phase = (uint32_t)r32(SEC_PHASE);
    const int sp = r16(SEC_PASSED), sc = r32(SEC_CURRENT);
    if (countdown > 0 && countdown <= 60 && r16(SEC_MODE) == 0 && phase >= 1 && phase <= 3 &&
        sp >= 0 && sp < 8 && sc >= 0 && sc < 8 && r32(SEC_REF + 4u * sc) >= 0 &&
        std::abs(r32(SEC_REF + 4u * (uint32_t)sp) - r32(SEC_TIME)) <= 30000) {
        const int32_t split = r32(SEC_REF + 4u * sp) - r32(SEC_TIME);
        const float a = std::fmin(1.f, countdown / 8.f);             /* fade out */
        const float e = std::fmin(1.f, (60 - countdown) / 4.f);      /* slide in */
        const float y = (show_rank ? 92.f : 26.f) - 6.f * (1.f - e);
        const ImU32 col = split <= 0 ? IM_COL32(74, 222, 128, (int)(255 * a))
                                     : IM_COL32(255, 92, 80, (int)(255 * a));
        char b[32];
        const int32_t m = split < 0 ? -split : split;
        std::snprintf(b, sizeof b, "%s%d.%03d", split <= 0 ? "\xE2\x88\x92" : "+", m / 1000, m % 1000);
        const ImVec2 a0 = C(540, y), a1 = C(740, y + 34);
        dl->AddRectFilled(a0, a1, rgba(8, 12, 18, 0.55f * a), 17.f * s);
        dl->AddRect(a0, a1, split <= 0 ? IM_COL32(52, 199, 89, (int)(140 * a)) : IM_COL32(255, 59, 48, (int)(140 * a)),
                    17.f * s, 0, 1.f * s);
        text(dl, SMALL, C(558, y + 9), rgba(255, 255, 255, 0.78f * a), std::string(tx.section) + " " + std::to_string(sp + 1));
        text(dl, MEDIUM, C(724, y + 5), col, b, 1);
    }

    /* Race messages (centre): wrong way, final lap, best lap */
    {
        const int32_t fr = r32(FRAMES);
        if (lap != g.prev_lap) {
            if (g.prev_lap > 0 && lap > g.prev_lap) {
                if (lap == laps && laps > 1) g.msg_final = fr;
                if (best >= 0 && times[lap - 2] == best && lap > 2) g.msg_best = fr;
            }
            g.prev_lap = lap;
        }
        const bool wrong_way = r16(CAR + CAR_WRONG_WAY) == 1 && kmh > 10;
        if (!wrong_way) g.wrong_way_since = -1;
        else if (g.wrong_way_since < 0) g.wrong_way_since = fr;
        auto banner = [&](const char *t, ImU32 border, ImU32 col, float alpha, float y) {
            /* at least 340 wide; longer translations widen it */
            const float half = std::fmax(170.f * s, text_width(DIGITS, t) * 0.5f + 26.f * s);
            const ImVec2 a0(xc + 640.f * s - half, y * s), a1(xc + 640.f * s + half, (y + 52) * s);
            dl->AddRectFilled(a0, a1, rgba(8, 12, 18, 0.62f * alpha), 26.f * s);
            dl->AddRect(a0, a1, (border & 0x00FFFFFFu) | ((ImU32)(alpha * 200) << 24), 26.f * s, 0, 2.f * s);
            text(dl, DIGITS, C(640, y + 1), (col & 0x00FFFFFFu) | ((ImU32)(alpha * 255) << 24), t, 0.5f);
        };
        auto fade = [&](int32_t start, int32_t duration) -> float {   /* 0..1: in, hold, out */
            if (start < 0) return 0.f;
            const int32_t t = fr - start;
            if (t < 0 || t > duration) return 0.f;
            return std::fmin(1.f, std::fmin(t / 4.f, (duration - t) / 8.f));
        };
        if (g.wrong_way_since >= 0 && fr - g.wrong_way_since > 30) {   /* after 1 s */
            const float pulse = 0.65f + 0.35f * std::sin((fr - g.wrong_way_since) * 0.35f);
            banner(tx.wrong_way, IM_COL32(255, 59, 48, 255), IM_COL32(255, 92, 80, 255), pulse, 250);
        } else if (float a = fade(g.msg_final, 75); a > 0.f) {
            banner(tx.final_lap, amber, amber, a, 250);
        } else if (float b = fade(g.msg_best, 75); b > 0.f) {
            banner(tx.new_best_lap, IM_COL32(52, 199, 89, 255), IM_COL32(74, 222, 128, 255), b, 250);
        }
    }

    /* Bottom right: tachometer, speed, gear */
    const ImVec2 c = R(1145, 581);
    const float rr = 110.f * s;
    dl->AddCircleFilled(c, 112.f * s, rgba(8, 12, 18, 0.55f), 64);
    const float a0 = 3.14159f * 0.75f, a1 = 3.14159f * 2.25f;      /* 270 degrees over the top */
    dl->PathArcTo(c, rr, a0, a1, 64);
    dl->PathStroke(rgba(255, 255, 255, 0.16f), 0, 10.f * s);
    const float f = std::fmin(1.f, std::fmax(0.f, rpm / 10000.f));
    if (f > 0.005f) {
        dl->PathArcTo(c, rr, a0, a0 + (a1 - a0) * f, 64);
        dl->PathStroke(f > 0.85f ? red : amber, 0, 10.f * s);
    }
    dl->PathArcTo(c, rr, a0 + (a1 - a0) * 0.85f, a1, 32);
    dl->PathStroke(rgba(255, 59, 48, 0.55f), 0, 4.f * s);
    for (int k = 0; k <= 10; k += 2) {
        const float a = a0 + (a1 - a0) * k / 10.f;
        text(dl, SMALL, ImVec2(c.x + std::cos(a) * 88.f * s, c.y + std::sin(a) * 88.f * s - 8.f * s),
             rgba(255, 255, 255, 0.8f), std::to_string(k), 0.5f);
    }
    text(dl, LARGE, ImVec2(c.x, c.y - 62.f * s), white, std::to_string(kmh < 0 ? 0 : kmh), 0.5f);
    text(dl, SMALL, ImVec2(c.x, c.y + 12.f * s), grey, tx.kmh, 0.5f);
    dl->AddRectFilled(ImVec2(c.x - 24 * s, c.y + 36 * s), ImVec2(c.x + 24 * s, c.y + 84 * s), amber, 10 * s);
    text(dl, DIGITS, ImVec2(c.x, c.y + 36 * s), IM_COL32(11, 16, 22, 255),
         gear <= 0 ? std::string("N") : std::to_string(gear), 0.5f);

    /* Bottom left: map */
    if (g.map >= 0) {
        const TrackMap &k = g.maps[g.map];
        panel(dl, P(28, 494), P(264, 694), s, 12);
        std::string name = k.name;
        for (char &ch : name) ch = ch == '_' ? ' ' : (char)toupper((unsigned char)ch);
        text(dl, SMALL, P(42, 504), grey, name);
        const float bw = 204.f * s, bh = 150.f * s;
        const float sc = std::fmin(bw / (k.x1 - k.x0), bh / (k.z1 - k.z0));
        const ImVec2 o = P(44, 528);
        /* orientation of the stock map: z up */
        auto M = [&](float x, float z) { return ImVec2(o.x + (x - k.x0) * sc, o.y + (k.z1 - z) * sc); };
        std::vector<ImVec2> pts;
        const size_t step = std::max<size_t>(1, k.pts.size() / 240);
        for (size_t i = 0; i < k.pts.size(); i += step) pts.push_back(M(k.pts[i].x, k.pts[i].y));
        dl->AddPolyline(pts.data(), (int)pts.size(), rgba(0, 0, 0, 0.55f), ImDrawFlags_Closed, 9.f * s);
        dl->AddPolyline(pts.data(), (int)pts.size(), rgba(255, 255, 255, 0.92f), ImDrawFlags_Closed, 4.f * s);
        const ImVec2 v = M((float)r32(CAR + 0x20u), (float)r32(CAR + 0x28u));
        dl->AddCircleFilled(v, 11.f * s, IM_COL32(255, 176, 32, 70));
        dl->AddCircleFilled(v, 6.f * s, amber);
        dl->AddCircle(v, 6.f * s, IM_COL32(11, 16, 22, 255), 0, 2.f * s);
    }
}

/* Split screen: one 120-line PS1 view per player, a compact block each (lap,
 * speed, gear, rpm); no map and no lap times (kept for player 1 only). */
void draw_hud_2p(int W, int H) {
    const Strings &tx = text_table();
    const float s = g_s * 0.72f;
    const ImU32 white = rgba(255, 255, 255, 1), grey = rgba(255, 255, 255, 0.72f),
                amber = IM_COL32(255, 176, 32, 255), red = IM_COL32(255, 59, 48, 255);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const int laps = std::max(1, std::min(8, (int)r32(LAP_COUNT)));
    const float saved = g_s;
    g_s = s;
    for (int j = 0; j < 2; ++j) {
        const uint32_t car = CAR + CAR_BYTES * j;
        const float y0 = H * 0.5f * j, y1 = y0 + H * 0.5f;
        const int lap = std::max(1, std::min(laps, (int)r16(car + 0x2AAu)));
        const int kmh = speed_shown(car), rpm = r32(car + 0x27Cu), gear = r16(car + 0x27Au);

        /* Top left of the view: player and lap */
        panel(dl, ImVec2(20 * s, y0 + 16 * s), ImVec2(196 * s, y0 + 70 * s), s);
        text(dl, SMALL, ImVec2(36 * s, y0 + 26 * s), j ? IM_COL32(34, 211, 238, 255) : amber,
             j ? tx.player2 : tx.player1);
        text(dl, SMALL, ImVec2(36 * s, y0 + 46 * s), grey, tx.lap);
        text(dl, MEDIUM, ImVec2(180 * s, y0 + 40 * s), white,
             std::to_string(lap) + " / " + std::to_string(laps), 1);

        /* Top right of the view: rank */
        const int cars = r16(CAR_COUNT);
        if (cars > 1 && cars <= 8) {
            const int rank = r16(car + CAR_RANK) + 1;
            panel(dl, ImVec2(W - 150 * s, y0 + 16 * s), ImVec2(W - 20 * s, y0 + 70 * s), s);
            text(dl, SMALL, ImVec2(W - 134 * s, y0 + 26 * s), grey, tx.position);
            text(dl, MEDIUM, ImVec2(W - 36 * s, y0 + 40 * s), rank == 1 ? amber : white,
                 std::to_string(rank) + " / " + std::to_string(cars), 1);
        }

        /* Bottom right of the view: rpm arc, speed, gear */
        const ImVec2 c(W - 110 * s, y1 - 100 * s);
        const float rr = 78 * s;
        dl->AddCircleFilled(c, 82 * s, rgba(8, 12, 18, 0.55f), 48);
        const float a0 = 3.14159f * 0.75f, a1 = 3.14159f * 2.25f;
        dl->PathArcTo(c, rr, a0, a1, 48);
        dl->PathStroke(rgba(255, 255, 255, 0.16f), 0, 8 * s);
        const float f = std::fmin(1.f, std::fmax(0.f, rpm / 10000.f));
        if (f > 0.005f) {
            dl->PathArcTo(c, rr, a0, a0 + (a1 - a0) * f, 48);
            dl->PathStroke(f > 0.85f ? red : amber, 0, 8 * s);
        }
        text(dl, DIGITS, ImVec2(c.x, c.y - 40 * s), white, std::to_string(kmh < 0 ? 0 : kmh), 0.5f);
        text(dl, SMALL, ImVec2(c.x, c.y + 6 * s), grey, tx.kmh, 0.5f);
        dl->AddRectFilled(ImVec2(c.x - 17 * s, c.y + 28 * s), ImVec2(c.x + 17 * s, c.y + 60 * s), amber, 7 * s);
        text(dl, MEDIUM, ImVec2(c.x, c.y + 32 * s), IM_COL32(11, 16, 22, 255),
             gear <= 0 ? std::string("N") : std::to_string(gear), 0.5f);
    }
    g_s = saved;
}

bool g_panels_env = true;                 /* PSX_R4_HUD=0: car effects only */
std::atomic<bool> g_panels_cmd{true};      /* debug command "r4.hud on|off" */

/* What the last presented frame drew, for the stock-HUD filters (emulation
 * thread): 0 no panels, 1 the full-screen HUD, 2 the split-screen blocks.
 * Stays 0 when the HUD layer never draws (no OpenGL, ImGui failed). */
std::atomic<int> s_panels_shown{0};
std::atomic<bool> s_map_shown{false};

void hud_draw(int W, int H, void *) {
    const bool hud = s_enabled.load(std::memory_order_relaxed);
    const bool effects = r4_car_effects_active();
    if (!hud && !effects) { s_panels_shown.store(0); return; }
    /* race, attract demo included (effects); panels outside the demo */
    if (psx_mod_read_half(RACE_WORD) != 0x180u) {
        if (g.was_in_race && effects) r4_car_effects_off_race();
        g.was_in_race = false;
        s_panels_shown.store(0);
        return;
    }
    const bool panels = hud && g_panels_env && g_panels_cmd.load(std::memory_order_relaxed) && in_race();
    if (!g.ctx) load_maps_once();
    {
        const uint32_t f = frame_handler();
        if (f != g.prev_handler) { g.prev_handler = f; g.was_in_race = false; }
        /* savestate loaded on another track: car far from the map */
        if (g.was_in_race && g.map >= 0 && ++g.check >= 120) {
            g.check = 0;
            const float x = (float)r32(CAR + 0x20u), z = (float)r32(CAR + 0x28u);
            float dmin = 1e30f;
            for (const ImVec2 &p : g.maps[g.map].pts) dmin = std::fmin(dmin, (p.x - x) * (p.x - x) + (p.y - z) * (p.y - z));
            if (dmin > 1500.f * 1500.f) g.was_in_race = false;
        }
    }
    if (!g.was_in_race) {   /* start: track from the car's position */
        g.prev_lap = 0; g.msg_final = g.msg_best = g.wrong_way_since = -1;
        g.map = split_screen() ? -1 : race_map((float)r32(CAR + 0x20u), (float)r32(CAR + 0x28u));
        g.was_in_race = true;
    }
    ImGuiContext *prev = ImGui::GetCurrentContext();
    if (!g.ctx) {
        g.ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(g.ctx);
        ImGui::GetIO().IniFilename = nullptr;
        g.gl = ImGui_ImplOpenGL3_Init("#version 330 core");
    }
    ImGui::SetCurrentContext(g.ctx);
    if (!g.gl) { ImGui::SetCurrentContext(prev); s_panels_shown.store(0); return; }
    if (!g.font) load_fonts();
    g_s = H / 720.f;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.DeltaTime = 1.f / 60.f;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    const bool split = split_screen();
    r4_car_effects_draw(ImGui::GetBackgroundDrawList(), W, H, split);
    if (panels) { if (split) draw_hud_2p(W, H); else draw_hud(W, H); }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::SetCurrentContext(prev);
    s_panels_shown.store(panels ? (split ? 2 : 1) : 0);
    s_map_shown.store(panels && !split && g.map >= 0);
}

/* ---------------------------------------------------------------- stock HUD
 * The stock HUD elements this HUD replaces are skipped by function filters
 * (return 1: the routine returns to its caller at once). Each one below only
 * builds GPU primitives at the scratchpad heap pointer *0x1F800000 and links
 * them into the ordering table (AddPrim, SetSprt, SetDrawMode helpers);
 * skipping it leaves the heap pointer and the table as they were. They are
 * void: the race handlers' call sites (overlay 660) load fresh arguments or
 * overwrite v0 right after each call. A filter only skips while the last
 * presented frame drew the replacement and the guest is still in that race
 * view, so menus, the attract demo, the filmed victory lap and a renderer
 * without the HUD layer keep the stock HUD.
 *
 * Kept, as they are not pure drawing or have no replacement here:
 *  - 0x8003C4B4 split-time popup and the top-left lap clock / section icon:
 *    it counts down the popup timer 0x8010C244 and plays the split sound;
 *  - 0x80039C04: final-lap sound and flag, draws nothing;
 *  - 0x80022048: toggles 0x8009EE68 (blink state);
 *  - 0x8002C0F8: stores the readout row in the HUD object (+0x226F6) that
 *    0x8002C194 reads; with 0x8002C194, a countdown readout this HUD lacks;
 *  - 0x80021774: five-digit figure this HUD does not show;
 *  - 0x8002258C: 104x16 sprite at the centre in race phases 1..3; nothing
 *    showed there in a Time Attack run, so no known replacement;
 *  - 0x80020A98 links the persistent label sprites; it is also the
 *    widescreen plugin's HUD-range closer, which a filter placed before it in
 *    the plan would skip, so it stays and the labels this HUD replaces are
 *    unlinked at DrawOTag instead (persistent labels, below);
 *  - 0x80021A20 sets the rank digit's texture in those persistent sprites
 *    and draws nothing itself;
 *  - 0x80034444 centre band, 0x80034F14 pause, 0x800559BC/0x80055968 replay
 *    and demo text. */
enum : uint32_t {
    STOCK_NEEDLE = 1u << 0,    /* 0x800212E0 tachometer needle, a0 rpm, a1 view */
    STOCK_GAUGE = 1u << 1,     /* 0x80021134 tachometer dial and frame, a0 view */
    STOCK_SPEED = 1u << 2,     /* 0x80021614 three-digit speed */
    STOCK_GEAR = 1u << 3,      /* 0x80021960 gear digit */
    STOCK_LAPS = 1u << 4,      /* 0x80021C38 LAP n/N cluster */
    STOCK_TIMES = 1u << 5,     /* 0x8002BEA4 lap time list and track record (+ RECORD, F labels) */
    STOCK_MAP = 1u << 6,       /* 0x80021DDC course minimap and car dots */
    STOCK_RANK = 1u << 7,      /* persistent RANK n | N cluster */
};
/* Replaced by the full-screen HUD / by the split-screen blocks (rpm arc,
 * speed, gear and lap, no map or lap times). */
constexpr uint32_t k_full_screen = STOCK_NEEDLE | STOCK_GAUGE | STOCK_SPEED | STOCK_GEAR | STOCK_LAPS |
                                   STOCK_TIMES | STOCK_MAP | STOCK_RANK;
constexpr uint32_t k_split_screen = STOCK_NEEDLE | STOCK_GAUGE | STOCK_SPEED | STOCK_GEAR | STOCK_LAPS;
std::atomic<uint32_t> g_skip_mask{0xFFFFFFFFu};   /* debug command "r4.hud skip MASK" */

int skip_stock(uint32_t element) {
    if (!s_enabled.load(std::memory_order_relaxed) || !(g_skip_mask.load(std::memory_order_relaxed) & element))
        return 0;
    const int shown = s_panels_shown.load(std::memory_order_relaxed);
    if (shown == 0 || !in_race()) return 0;
    const bool split = split_screen();
    if (shown == 1 && !split) {
        if (element == STOCK_MAP && !s_map_shown.load(std::memory_order_relaxed)) return 0;
        if (element == STOCK_RANK && !s_rank_shown.load(std::memory_order_relaxed)) return 0;
        return (k_full_screen & element) != 0;
    }
    if (shown == 2 && split) return (k_split_screen & element) != 0;
    return 0;
}

int skip_needle(CPUState *, uint32_t) { return skip_stock(STOCK_NEEDLE); }
int skip_gauge(CPUState *, uint32_t) { return skip_stock(STOCK_GAUGE); }
int skip_speed(CPUState *, uint32_t) { return skip_stock(STOCK_SPEED); }
int skip_gear(CPUState *, uint32_t) { return skip_stock(STOCK_GEAR); }
int skip_laps(CPUState *, uint32_t) { return skip_stock(STOCK_LAPS); }
int skip_times(CPUState *, uint32_t) { return skip_stock(STOCK_TIMES); }
int skip_map(CPUState *, uint32_t) { return skip_stock(STOCK_MAP); }

struct StockFilter { uint32_t address; PSXModFunctionFilterCallback fn; };
const StockFilter k_stock_filters[] = {
    {0x800212E0u, skip_needle}, {0x80021134u, skip_gauge}, {0x80021614u, skip_speed},
    {0x80021960u, skip_gear},   {0x80021C38u, skip_laps},  {0x8002BEA4u, skip_times},
    {0x80021DDCu, skip_map},
};
constexpr int k_stock_filter_count = (int)(sizeof k_stock_filters / sizeof k_stock_filters[0]);
int s_stock_hooks;

/* ---------------------------------------------------------------- persistent labels
 * The RANK n | N cluster and the RECORD and F labels are not built per frame:
 * they are 20-byte sprite packets in each frame buffer's tail (buffer +0x22688,
 * one chain set up at race start; 0x80021A20 only changes the rank digit's
 * texture), which 0x80020A98 links into an ordering-table slot every frame
 * (slot -> +0x22688 -> ... -> the slot's previous contents). At DrawOTag,
 * before the GPU walks the table, the slot is pointed past the labels this
 * HUD replaces. The labels kept (TIME LIMIT, ...) stay linked in order; a kept
 * label whose successor changes is drawn from a copy in a tail entry the race
 * leaves empty, so the game's own packets are never modified and nothing has
 * to be restored: the slot is rebuilt next frame and the copy, unlinked, is
 * inert. Without a free entry the labels stay. The widescreen plugin's tail
 * walk anchors the copies like the originals. */
constexpr uint32_t BUF0 = 0x800ADCA0u, BUF_STRIDE = 0x22778u;
constexpr uint32_t OT1_SLOT = 0x70u, OT1_HEAD = 0xB6Cu, OT2_SLOT = 0xB70u, OT2_HEAD = 0x166Cu;
constexpr uint32_t TAIL_FIRST = 0x22688u, TAIL_END = 0x22778u, TAIL_ENTRY = 20u;
constexpr int TAIL_ENTRIES = (int)((TAIL_END - TAIL_FIRST) / TAIL_ENTRY);   /* 12 */

/* USA layout, 320x240 race screen (measured in a Grand Prix race). */
struct StockLabel { int16_t x, y, w, h; uint32_t element; };
const StockLabel k_stock_labels[] = {
    {12, 14, 56, 12, STOCK_RANK},     /* RANK */
    {12, 26, 24, 24, STOCK_RANK},     /* rank digit (texture set by 0x80021A20) */
    {32, 26, 8, 24, STOCK_RANK},      /* | */
    {44, 32, 16, 16, STOCK_RANK},     /* entrants */
    {230, 14, 84, 12, STOCK_TIMES},   /* RECORD */
    {231, 28, 12, 12, STOCK_TIMES},   /* F (before the record time) */
};

struct TailCopy { uint32_t address = 0; uint32_t words[5] = {}; };
TailCopy s_copies[2][TAIL_ENTRIES];   /* per buffer: the copies last written */
std::atomic<int> s_labels_hidden{0};  /* labels unlinked in the last frame (state) */

uint32_t label_element(uint32_t packet) {
    const uint32_t tag = psx_mod_read_word(packet), code = psx_mod_read_word(packet + 4u);
    if ((tag >> 24) != 4u || (code >> 24) < 0x64u || (code >> 24) > 0x67u) return 0;   /* sprite */
    const uint32_t xy = psx_mod_read_word(packet + 8u), wh = psx_mod_read_word(packet + 16u);
    for (const StockLabel &l : k_stock_labels)
        if ((int16_t)xy == l.x && (int16_t)(xy >> 16) == l.y && (int16_t)wh == l.w && (int16_t)(wh >> 16) == l.h)
            return l.element;
    return 0;
}

void unlink_stock_labels(CPUState *cpu, uint32_t) {
    int b = -1;
    uint32_t slot = 0;
    for (int k = 0; k < 2 && b < 0; ++k) {
        const uint32_t base = BUF0 + BUF_STRIDE * (uint32_t)k;
        if (cpu->gpr[4] == base + OT1_HEAD) { b = k; slot = base + OT1_SLOT; }
        else if (cpu->gpr[4] == base + OT2_HEAD) { b = k; slot = base + OT2_SLOT; }
    }
    if (b < 0) return;
    uint32_t hide = 0;
    if (skip_stock(STOCK_RANK)) hide |= STOCK_RANK;
    if (skip_stock(STOCK_TIMES)) hide |= STOCK_TIMES;
    if (!hide) { s_labels_hidden.store(0, std::memory_order_relaxed); return; }
    const uint32_t base = BUF0 + BUF_STRIDE * (uint32_t)b, first = base + TAIL_FIRST, end = base + TAIL_END;
    auto entry = [&](uint32_t link) -> uint32_t {   /* 24-bit link -> tail entry address, or 0 */
        const uint32_t a = 0x80000000u | (link & 0xFFFFFFu);
        return a >= first && a < end && (a - first) % TAIL_ENTRY == 0 ? a : 0;
    };
    /* The labels' link: the slot itself, or (primitives added to the slot
     * after 0x80020A98) the packet before them in the slot's list. Either
     * word is written by this frame's 0x80020A98 or AddPrim. */
    uint32_t slot_word = psx_mod_read_word(slot);
    for (int step = 0; !entry(slot_word); ++step) {
        const uint32_t next = slot_word & 0xFFFFFFu;
        if (step == 256 || next == 0xFFFFFFu || next >= 0x200000u || (next & 3u)) return;
        slot = 0x80000000u | next;
        slot_word = psx_mod_read_word(slot);
    }
    /* the chain: tail entries from the slot until the first link out of the tail */
    uint32_t chain[TAIL_ENTRIES], kept[TAIL_ENTRIES];
    int n = 0, nk = 0, hidden = 0;
    uint32_t link = slot_word;
    bool in_chain[TAIL_ENTRIES] = {};
    while (const uint32_t a = entry(link)) {
        const int i = (int)((a - first) / TAIL_ENTRY);
        if (in_chain[i] || n == TAIL_ENTRIES) return;   /* not the race chain */
        in_chain[i] = true;
        chain[n++] = a;
        link = psx_mod_read_word(a);
    }
    const uint32_t terminal = link & 0xFFFFFFu;
    for (int i = 0; i < n; ++i) {
        if (label_element(chain[i]) & hide) ++hidden;
        else kept[nk++] = chain[i];
    }
    if (!hidden) return;
    /* kept[j..] already link in order to the terminal; kept[0..j-1] need copies */
    int j = nk;
    for (int i = nk - 1; i >= 0; --i) {
        const uint32_t next = i + 1 < nk ? kept[i + 1] & 0xFFFFFFu : terminal;
        if ((psx_mod_read_word(kept[i]) & 0xFFFFFFu) != next) break;
        j = i;
    }
    TailCopy *copies = s_copies[b];
    uint32_t to[TAIL_ENTRIES];
    int nc = 0;
    for (int e = 0; e < TAIL_ENTRIES && nc < j; ++e) {
        if (in_chain[e]) continue;
        const uint32_t a = first + TAIL_ENTRY * (uint32_t)e;
        bool empty = true, ours = copies[e].address == a;
        for (uint32_t w = 0; w < 5u; ++w) {
            const uint32_t v = psx_mod_read_word(a + 4u * w);
            empty = empty && v == 0u;
            ours = ours && v == copies[e].words[w];
        }
        if (empty || ours) to[nc++] = a;
    }
    if (nc < j) return;   /* no room for the copies: keep the stock labels */
    for (int i = j - 1; i >= 0; --i) {
        const uint32_t next = i + 1 < j ? to[i + 1] : j < nk ? kept[j] : (0x80000000u | terminal);
        TailCopy &c = copies[(to[i] - first) / TAIL_ENTRY];
        c.address = to[i];
        for (uint32_t w = 0; w < 5u; ++w) c.words[w] = psx_mod_read_word(kept[i] + 4u * w);
        c.words[0] = (c.words[0] & 0xFF000000u) | (next & 0xFFFFFFu);
        for (uint32_t w = 0; w < 5u; ++w) psx_mod_write_word(to[i] + 4u * w, c.words[w]);
    }
    const uint32_t head = j > 0 ? to[0] : nk > 0 ? kept[0] : terminal;
    psx_mod_write_word(slot, (slot_word & 0xFF000000u) | (head & 0xFFFFFFu));
    s_labels_hidden.store(hidden, std::memory_order_relaxed);
}

/* r4.hud: on | off | skip MASK | state */
int hud_command(const char *args, char *reply, int cap) {
    char word[16] = "";
    std::sscanf(args, "%15s", word);
    unsigned mask = 0;
    if (!std::strcmp(word, "on") || !std::strcmp(word, "off")) {
        g_panels_cmd.store(word[1] == 'n');
        std::snprintf(reply, (size_t)cap, "panels %s", word);
        return 1;
    }
    if (!std::strcmp(word, "skip") && std::sscanf(args, "%*s %x", &mask) == 1) {
        g_skip_mask.store(mask);
        std::snprintf(reply, (size_t)cap, "skip mask 0x%X", mask);
        return 1;
    }
    if (!std::strcmp(word, "state")) {
        std::snprintf(reply, (size_t)cap,
                      "active %d panels %d shown %d map %d rank %d split %d skip 0x%X hooks %d/%d labels hidden %d",
                      (int)s_enabled.load(), (int)g_panels_cmd.load(), s_panels_shown.load(),
                      (int)s_map_shown.load(), (int)s_rank_shown.load(), (int)split_screen(), g_skip_mask.load(),
                      s_stock_hooks, k_stock_filter_count + 1, s_labels_hidden.load());
        return 1;
    }
    std::snprintf(reply, (size_t)cap, "on | off | skip MASK (hex) | state");
    return 0;
}

}  // namespace
void r4_race_hud_bind() {
    psx_host_set_hud_draw_cb(hud_draw, nullptr);
}

static void r4_race_hud_activate(void) {
    r4_race_hud_bind();
    s_enabled.store(true, std::memory_order_relaxed);
    if (g_panels_env && s_stock_hooks != k_stock_filter_count + 1)
        std::fprintf(stderr, "[r4-race-hud] %d of %d stock HUD hooks registered\n", s_stock_hooks,
                     k_stock_filter_count + 1);
}

/* Host-only: a plan without this plugin (or a netplay match) starts with the
 * HUD off. */
static void r4_race_hud_session_reset(void) {
    s_enabled.store(false, std::memory_order_relaxed);
    s_panels_shown.store(0);
    /* The framework clears session-owned HUD callbacks before invoking reset
     * hooks. Keep this shared draw layer installed for Car Effects even when
     * the optional Race HUD package itself is off. */
    psx_host_set_hud_draw_cb(hud_draw, nullptr);
}

PSX_MOD_CONSTRUCTOR(r4_register_race_hud) {
    /* Plugin activation clears session-owned callbacks. Each active feature
     * reattaches this shared layer, including Effects with the HUD disabled. */
    const char *e = std::getenv("PSX_R4_HUD");
    g_panels_env = !(e && e[0] == '0');
    (void)psx_mod_register_activation_plugin(PLUGIN_ID, r4_race_hud_activate);
    (void)psx_mod_register_session_reset_plugin(PLUGIN_ID, r4_race_hud_session_reset);
    (void)psx_mod_register_command("r4.hud", hud_command);
    if (g_panels_env) {
        for (const StockFilter &f : k_stock_filters)
            s_stock_hooks += psx_mod_register_function_filter_plugin(PLUGIN_ID, f.address, f.fn);
        /* DrawOTag (in game.toml mod_function_entry_funcs) */
        s_stock_hooks += psx_mod_register_function_entry_plugin(PLUGIN_ID, 0x80093520u, unlink_stock_labels);
    }
}
