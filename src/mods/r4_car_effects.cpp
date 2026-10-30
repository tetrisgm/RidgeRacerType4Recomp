#include "r4_race_hud.h"
/* Car effects for R4, drawn by the host on the HUD layer, under the race HUD,
 * from the race state (r4_race_state.h).
 *
 * Textured particles (procedural textures created on the first draw: fractal
 * noise smoke puffs, a fireball) live in world space and are projected on
 * every presented frame:
 *  - exhaust backfires: on an upshift, as pops when lifting off at high rpm
 *    and on the rev limiter; tongues of flame blown backwards and stretched
 *    along their motion, a white-hot core, embers that fall, a puff of dark
 *    smoke;
 *  - drift smoke: emitted continuously at each rear tyre's contact patch
 *    along its path, dense and hugging the ground at birth, then spreading,
 *    rising and thinning into a plume, tinted by the scene brightness;
 *  - sparks on impacts.
 * There is no depth test: only nearby cars seen from behind (or three
 * quarters) emit. PSX_R4_EFFECTS=0 turns the effects off;
 * PSX_R4_EFFECTS_DEBUG=1 draws the alignment grid.
 *
 * The same plugin drives the live uniforms of the lighting preset
 * (postfx/r4_2026/lighting.pfx).
 *
 * Visual scale: 1 m is about 125 world units (alignment grid on the player's
 * car); a car's origin is at the centre of its rear face, at tail-light
 * height. Forward = (sin heading, 0, cos heading); y is down. */
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "imgui.h"

extern "C" {
#include "host_overlay.h"
#include "mod_plugins.h"
#include "r4_race_state.h"
}
#include <SDL3/SDL.h>

#include "r4_car_effects.h"

#define PLUGIN_ID "r4.car-effects"

namespace {

std::atomic<bool> s_enabled{false};

constexpr float M = R4_METER;               /* world units per meter */
constexpr float K_ANGLE = 6.28318531f / 4096.f;

/* Car frame (meters: right, up, forward) from the origin, on the axis about
 * 0.9 m ahead of the rear face and 0.55 m above the ground; side-on alignment
 * in the lab (frozen camera, car driving past, PSX_R4_EFFECTS_DEBUG=1): rear
 * face -0.9 m, rear wheels -0.4 m. */
constexpr float EXHAUST_BACK = -0.95f, EXHAUST_UP = -0.1f, EXHAUST_SIDE = 0.35f;
constexpr float WHEEL_BACK = -0.4f, WHEEL_SIDE = 0.8f, GROUND = -0.55f;
constexpr float MIDDLE = 1.3f;   /* middle of the car (side scrapes) */

enum Kind : uint8_t { SMOKE, EXHAUST_SMOKE, FIRE, EMBER, SPARK };

struct Particle {
    float x, y, z;        /* world */
    float vx, vy, vz;     /* world units per second */
    float age, life;
    float r0, r1;         /* birth and final radius (meters) */
    float rot, vrot;      /* texture rotation (radians) */
    float strength;       /* intensity 0..1 */
    float phase;          /* turbulence */
    float jx, jy, jz;     /* fire: jet velocity relative to the car (stretch) */
    Kind kind;
    uint8_t variant;
    uint8_t car;          /* emitting car (occlusion by its body) */
};

/* A pending burst of backfires on one car: spaced pops. */
struct Burst { int remaining = 0; float wait = 0.f, strength = 0.f; };

/* OpenGL entry points (render context, HUD layer). */
struct Gl {
    void (*BlendFunc)(unsigned, unsigned) = nullptr;
    void (*GenTextures)(int, unsigned *) = nullptr;
    void (*BindTexture)(unsigned, unsigned) = nullptr;
    void (*TexParameteri)(unsigned, unsigned, int) = nullptr;
    void (*TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = nullptr;
    void (*GenerateMipmap)(unsigned) = nullptr;
    void (*GetIntegerv)(unsigned, int *) = nullptr;
    void (*GenBuffers)(int, unsigned *) = nullptr;
    void (*BindBuffer)(unsigned, unsigned) = nullptr;
    void (*BufferData)(unsigned, ptrdiff_t, const void *, unsigned) = nullptr;
    void (*ReadPixels)(int, int, int, int, unsigned, unsigned, void *) = nullptr;
    void *(*MapBufferRange)(unsigned, ptrdiff_t, ptrdiff_t, unsigned) = nullptr;
    unsigned char (*UnmapBuffer)(unsigned) = nullptr;
    void (*PixelStorei)(unsigned, int) = nullptr;
} gl;

constexpr int SMOKE_TEXTURES = 4;

struct State {
    bool env_read = false, active = true, debug = false, textures_tried = false;
    std::vector<Particle> parts;
    uint32_t frame = 0;
    float px[R4_RACE_CARS] = {}, pz[R4_RACE_CARS] = {}, py[R4_RACE_CARS] = {};
    float vx[R4_RACE_CARS] = {}, vy[R4_RACE_CARS] = {}, vz[R4_RACE_CARS] = {};
    bool known[R4_RACE_CARS] = {};
    uint32_t vblank = 0;                           /* game VBlanks at the last sampled frame */
    float cam_p[3] = {}, cam_v[3] = {};            /* view 0 eye and its velocity (world / s) */
    bool cam_known = false;
    float measured_kmh = 0.f, computed_kmh = 0.f;  /* lab: speedometer and computed speed (player) */
    int spark_seen[R4_RACE_CARS] = {};             /* last impact already emitted (frame) */
    int32_t max_rpm[R4_RACE_CARS] = {};            /* highest rpm seen (rev limiter), per race */
    int32_t prev_rpm[R4_RACE_CARS] = {};
    Burst burst[R4_RACE_CARS];
    float drift_strength[R4_RACE_CARS] = {};       /* 0..1, drift smoke in progress */
    float wheel_x[R4_RACE_CARS][2] = {}, wheel_y[R4_RACE_CARS][2] = {}, wheel_z[R4_RACE_CARS][2] = {};
    bool wheel_ok[R4_RACE_CARS][2] = {};
    unsigned tex_smoke[SMOKE_TEXTURES] = {}, tex_fire = 0;
    /* scene brightness (asynchronous readback of three lines of the frame) */
    unsigned pbo[2] = {};
    int pbo_w = 0, pbo_i = 0, pbo_ready = 0;
    float ambient = 0.8f;
    /* game frames between the sampled state and the displayed frame (PSX_R4_EFFECTS_DELAY) */
    int delay = 1;
} g;

/* Lab (TCP command "r4.effects", debug-server thread): requests read by the
 * render thread. */
std::atomic<int> g_lab_flame{0};         /* car + 1, strength in thousandths << 8 */
std::atomic<int> g_lab_smoke{0};         /* car + 1; smoke forced while non-zero */
std::atomic<int> g_lab_everywhere{0};    /* 1: effects seen from anywhere (no "from behind" filter) */
std::atomic<int> g_lab_debug{-1}, g_lab_delay{-1}, g_lab_active{-1};
std::atomic<unsigned> g_last_vblanks{0};  /* VBlanks of the last game frame (lab) */

uint32_t g_rng = 0x9E3779B9u;
float rnd() {   /* xorshift: fast, no state shared with rand() */
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return (g_rng >> 8) * (1.f / 16777216.f);
}
float rnd_signed() { return rnd() * 2.f - 1.f; }

/* car-frame point (meters: right, up, forward) -> world */
void car_to_world(const R4RaceCar &v, float right, float up, float fwd,
                  float &x, float &y, float &z) {
    const float c = std::cos(v.heading * K_ANGLE), s = std::sin(v.heading * K_ANGLE);
    x = (float)v.x + (s * fwd + c * right) * M;
    z = (float)v.z + (c * fwd - s * right) * M;
    y = (float)v.y - up * M;
}

/* Without a depth test an effect is shown only where neither the car itself
 * nor distant scenery can hide it: camera behind the car (cone of half-angle
 * `cone_deg`) and car nearby. */
bool seen_from_behind(const R4RaceCamera &cam, const R4RaceCar &v, float range_m, float cone_deg = 60.f) {
    if (g_lab_everywhere.load(std::memory_order_relaxed)) return true;
    const float dx = (float)v.x - cam.eye[0], dz = (float)v.z - cam.eye[2];
    const float d = std::sqrt(dx * dx + dz * dz);
    if (d > range_m * M) return false;
    /* cockpit or bumper view: eye near the car looking along its axis (model not drawn) */
    if (d < 1.5f * M) {
        const float ax = std::sin(cam.yaw * K_ANGLE), az = std::cos(cam.yaw * K_ANGLE);
        if (ax * std::sin(v.heading * K_ANGLE) + az * std::cos(v.heading * K_ANGLE) > 0.8f) return false;
    }
    const float fx = std::sin(v.heading * K_ANGLE), fz = std::cos(v.heading * K_ANGLE);
    return (dx * fx + dz * fz) >= std::cos(cone_deg * 0.0174533f) * d;
}

/* view PS1 pixels -> window pixels (PS1 image scaled and centred, widescreen
 * margins included) */
struct Screen { float ox, oy, k, m; };
Screen screen_map(int W, int H) {
    const float m = (float)psx_mod_widescreen_view_x_margin();
    const float lw = 320.f + 2.f * m;
    const float k = std::fmin(W / lw, H / 240.f);
    return {(W - lw * k) * 0.5f, (H - 240.f * k) * 0.5f, k, m};
}

bool project(const R4RaceState &c, int view, const Screen &e, float x, float y, float z, ImVec2 &p, float &depth) {
    float sx, sy;
    if (!r4_race_project(&c, view, x, y, z, &sx, &sy, &depth)) return false;
    p = ImVec2(e.ox + (sx + e.m) * e.k, e.oy + sy * e.k);
    return true;
}

/* on-screen size of a radius (meters) at depth `depth` (camera units = world / 4) */
float screen_radius(const R4RaceState &c, int view, const Screen &e, float meters, float depth) {
    return (float)c.cameras[view].h * (meters * M / 4.f) / depth * e.k;
}

void additive_blend(const ImDrawList *, const ImDrawCmd *) {
    if (gl.BlendFunc) gl.BlendFunc(0x0302 /* GL_SRC_ALPHA */, 0x0001 /* GL_ONE */);
}

/* ---------------------------------------------------------------- procedural textures */

float hash(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFFu) / 65535.f;
}
/* smoothed value noise, periodic with period `per` */
float noise(float x, float y, int per, int seed) {
    const int xi = (int)std::floor(x), yi = (int)std::floor(y);
    const float fx = x - xi, fy = y - yi;
    const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    auto h = [&](int a, int b) { return hash(((a % per) + per) % per, ((b % per) + per) % per, seed); };
    const float a = h(xi, yi), b = h(xi + 1, yi), c = h(xi, yi + 1), d = h(xi + 1, yi + 1);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
float fbm(float x, float y, int seed) {
    float s = 0, amp = 0.5f, f = 1;
    for (int o = 0; o < 5; ++o) { s += amp * noise(x * f, y * f, 4 * (int)f, seed + o * 17); amp *= 0.5f; f *= 2; }
    return s;
}
float smooth(float a, float b, float x) { float t = std::fmin(1.f, std::fmax(0.f, (x - a) / (b - a))); return t * t * (3 - 2 * t); }

unsigned texture_rgba(const std::vector<uint8_t> &px, int n) {
    unsigned t = 0;
    int previous = 0;
    gl.GetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, &previous);
    gl.GenTextures(1, &t);
    gl.BindTexture(0x0DE1, t);
    gl.TexParameteri(0x0DE1, 0x2801, gl.GenerateMipmap ? 0x2703 : 0x2601);   /* MIN: trilinear */
    gl.TexParameteri(0x0DE1, 0x2800, 0x2601);                                /* MAG: linear */
    gl.TexParameteri(0x0DE1, 0x2802, 0x812F);
    gl.TexParameteri(0x0DE1, 0x2803, 0x812F);
    gl.TexImage2D(0x0DE1, 0, 0x8058 /* RGBA8 */, n, n, 0, 0x1908, 0x1401, px.data());
    if (gl.GenerateMipmap) gl.GenerateMipmap(0x0DE1);
    gl.BindTexture(0x0DE1, (unsigned)previous);
    return t;
}

/* Smoke puff: a cottony mass with a ragged edge (fractal noise), lit from above. */
unsigned make_smoke(int seed) {
    const int n = 128;
    std::vector<uint8_t> px((size_t)n * n * 4);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float u = (x + 0.5f) / n * 2 - 1, v = (y + 0.5f) / n * 2 - 1;
            const float d = std::sqrt(u * u + v * v);
            const float b1 = fbm(u * 1.6f + 3, v * 1.6f + 3, seed);
            const float b2 = fbm(u * 3.2f + 7, v * 3.2f + 1, seed + 101);
            const float mass = smooth(1.0f, 0.25f, d + 0.55f * (b1 - 0.5f));
            float a = mass * (0.55f + 0.6f * b2);
            a = std::fmin(1.f, a);
            const float light = 0.78f + 0.22f * smooth(0.6f, -0.6f, v + 0.3f * (b2 - 0.5f));   /* lighter on top */
            const size_t i = ((size_t)y * n + x) * 4;
            px[i] = px[i + 1] = (uint8_t)(255 * light);
            px[i + 2] = (uint8_t)(255 * std::fmin(1.f, light * 1.02f));
            px[i + 3] = (uint8_t)(255 * a);
        }
    return texture_rgba(px, n);
}

/* Fireball: a solid core with a flickering edge (noise), tinted by the vertex colour. */
unsigned make_fire() {
    const int n = 64;
    std::vector<uint8_t> px((size_t)n * n * 4);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float u = (x + 0.5f) / n * 2 - 1, v = (y + 0.5f) / n * 2 - 1;
            const float d = std::sqrt(u * u + v * v);
            const float b = fbm(u * 2.2f + 5, v * 2.2f + 9, 77);
            const float a = std::pow(smooth(1.0f, 0.0f, d + 0.45f * (b - 0.5f)), 1.4f);
            const size_t i = ((size_t)y * n + x) * 4;
            px[i] = px[i + 1] = px[i + 2] = 255;
            px[i + 3] = (uint8_t)(255 * a);
        }
    return texture_rgba(px, n);
}

void load_gl() {
    auto f = [](const char *n) { return SDL_GL_GetProcAddress(n); };
    gl.BlendFunc = (void (*)(unsigned, unsigned))f("glBlendFunc");
    gl.GenTextures = (void (*)(int, unsigned *))f("glGenTextures");
    gl.BindTexture = (void (*)(unsigned, unsigned))f("glBindTexture");
    gl.TexParameteri = (void (*)(unsigned, unsigned, int))f("glTexParameteri");
    gl.TexImage2D = (void (*)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *))f("glTexImage2D");
    gl.GenerateMipmap = (void (*)(unsigned))f("glGenerateMipmap");
    gl.GetIntegerv = (void (*)(unsigned, int *))f("glGetIntegerv");
    gl.GenBuffers = (void (*)(int, unsigned *))f("glGenBuffers");
    gl.BindBuffer = (void (*)(unsigned, unsigned))f("glBindBuffer");
    gl.BufferData = (void (*)(unsigned, ptrdiff_t, const void *, unsigned))f("glBufferData");
    gl.ReadPixels = (void (*)(int, int, int, int, unsigned, unsigned, void *))f("glReadPixels");
    gl.MapBufferRange = (void *(*)(unsigned, ptrdiff_t, ptrdiff_t, unsigned))f("glMapBufferRange");
    gl.UnmapBuffer = (unsigned char (*)(unsigned))f("glUnmapBuffer");
    gl.PixelStorei = (void (*)(unsigned, int))f("glPixelStorei");
}

bool textures_ready() {
    if (g.textures_tried) return g.tex_fire != 0;
    g.textures_tried = true;
    if (!gl.GenTextures || !gl.BindTexture || !gl.TexParameteri || !gl.TexImage2D || !gl.GetIntegerv) return false;
    for (int k = 0; k < SMOKE_TEXTURES; ++k) g.tex_smoke[k] = make_smoke(11 + k * 31);
    g.tex_fire = make_fire();
    return g.tex_fire != 0;
}

/* Average scene brightness, read without stalling (two readback buffers, one
 * frame late) on three lines across the middle of the frame: the smoke takes
 * the light of the place (tunnel, night, daylight). */
void measure_ambient(int W, int H) {
    if (!gl.GenBuffers || !gl.BindBuffer || !gl.BufferData || !gl.ReadPixels || !gl.MapBufferRange ||
        !gl.UnmapBuffer)
        return;
    const unsigned PACK = 0x88EB;
    const ptrdiff_t size = (ptrdiff_t)W * 4 * 3;
    if (!g.pbo[0] || g.pbo_w != W) {
        if (!g.pbo[0]) gl.GenBuffers(2, g.pbo);
        for (unsigned b : g.pbo) { gl.BindBuffer(PACK, b); gl.BufferData(PACK, size, nullptr, 0x88E1 /* STREAM_READ */); }
        g.pbo_w = W; g.pbo_ready = 0;
    }
    int alignment = 4;
    gl.GetIntegerv(0x0D05, &alignment);
    if (gl.PixelStorei) gl.PixelStorei(0x0D05, 4);
    gl.BindBuffer(PACK, g.pbo[g.pbo_i]);
    const int ys[3] = {H * 35 / 100, H * 55 / 100, H * 75 / 100};   /* GL: bottom at 0 */
    for (int k = 0; k < 3; ++k)
        gl.ReadPixels(0, ys[k], W, 1, 0x1908, 0x1401, (void *)(intptr_t)(k * W * 4));
    if (gl.PixelStorei) gl.PixelStorei(0x0D05, alignment);
    if (g.pbo_ready) {
        gl.BindBuffer(PACK, g.pbo[g.pbo_i ^ 1]);
        const uint8_t *p = (const uint8_t *)gl.MapBufferRange(PACK, 0, size, 0x0001 /* READ */);
        if (p) {
            double s = 0; int n = 0;
            for (int k = 0; k < 3; ++k)
                for (int x = W / 8; x < W * 7 / 8; x += 8, ++n) {
                    const uint8_t *q = p + ((size_t)k * W + x) * 4;
                    s += 0.3 * q[0] + 0.55 * q[1] + 0.15 * q[2];
                }
            gl.UnmapBuffer(PACK);
            const float l = n ? (float)(s / n / 255.0) : 0.5f;
            g.ambient += (std::fmin(1.f, 0.25f + 1.6f * l) - g.ambient) * 0.1f;
        }
    }
    gl.BindBuffer(PACK, 0);
    g.pbo_ready = 1;
    g.pbo_i ^= 1;
}

/* ---------------------------------------------------------------- emission */

/* One backfire out of each tailpipe: flame tongues, core, embers, dark smoke.
 * `lead`: time between the car state `v` and the next advance() (a game
 * frame when emitted from that frame's events, which are advanced right
 * after): the particles start that much younger, so they leave the pipe of
 * the car on screen. Born at age 0 they were advanced a whole game frame at
 * the car's speed before the car moved on: up to 1.5 m ahead of the
 * tailpipes, cores and embers drawn over the boot for a frame. */
void backfire(int i, const R4RaceCar &v, float strength, float lead) {
    const float fx = std::sin(v.heading * K_ANGLE), fz = std::cos(v.heading * K_ANGLE);
    for (int side = -1; side <= 1; side += 2) {
        float x, y, z;
        car_to_world(v, side * EXHAUST_SIDE, EXHAUST_UP, EXHAUST_BACK, x, y, z);
        /* core: a brief fireball that moves with the car */
        {
            Particle p{};
            p.x = x; p.y = y; p.z = z;
            p.vx = g.vx[i]; p.vy = g.vy[i]; p.vz = g.vz[i];
            p.life = 0.04f + 0.03f * strength; p.r0 = 0.07f + 0.05f * strength; p.r1 = p.r0 * 1.4f;
            p.rot = rnd() * 6.28f; p.strength = strength; p.kind = FIRE; p.phase = -1.f;   /* -1: core */
            p.age = 0.001f - lead;
            p.car = (uint8_t)i;
            g.parts.push_back(p);
        }
        /* tongues: jets blown backwards that open up and cool down */
        const int n = 7 + (int)(9 * strength);
        for (int k = 0; k < n; ++k) {
            Particle p{};
            p.x = x; p.y = y; p.z = z;
            /* narrow jet: 6 to 14 m/s along the axis, about ten degrees wide */
            const float speed = (6.f + 8.f * strength * (0.5f + rnd())) * M;
            const float ex = rnd_signed() * 0.9f * M, ey = -rnd() * 0.4f * M;
            p.jx = -fx * speed + fz * ex; p.jz = -fz * speed - fx * ex; p.jy = ey;
            p.vx = g.vx[i] + p.jx; p.vz = g.vz[i] + p.jz; p.vy = g.vy[i] + p.jy;
            p.life = 0.05f + 0.08f * rnd() * (0.6f + strength);
            p.r0 = 0.04f + 0.03f * strength; p.r1 = 0.1f + 0.1f * strength;
            p.rot = rnd() * 6.28f; p.vrot = rnd_signed() * 8.f; p.strength = strength; p.kind = FIRE;
            p.age = -rnd() * 0.03f - lead;   /* spread over 30 ms */
            p.car = (uint8_t)i;
            g.parts.push_back(p);
        }
        /* embers: blown backwards with the jet, barely lifted, falling as
         * streaks (launched upwards at 1-3 m/s they were drawn over the boot) */
        const int ne = 2 + (int)(5 * strength);
        for (int k = 0; k < ne; ++k) {
            Particle p{};
            p.x = x; p.y = y; p.z = z;
            const float speed = (4.f + 6.f * rnd()) * M;
            p.vx = g.vx[i] - fx * speed + rnd_signed() * 1.8f * M;
            p.vz = g.vz[i] - fz * speed + rnd_signed() * 1.8f * M;
            p.vy = g.vy[i] - (0.2f + 0.8f * rnd()) * M;
            p.life = 0.2f + 0.3f * rnd(); p.strength = strength; p.kind = EMBER;
            p.age = 0.001f - lead;
            p.car = (uint8_t)i;
            g.parts.push_back(p);
        }
        /* dark combustion smoke */
        for (int k = 0; k < 2; ++k) {
            Particle p{};
            p.x = x; p.y = y; p.z = z;
            p.vx = g.vx[i] * 0.8f - fx * 1.5f * M + rnd_signed() * 0.4f * M;
            p.vz = g.vz[i] * 0.8f - fz * 1.5f * M + rnd_signed() * 0.4f * M;
            p.vy = -0.3f * M;
            p.life = 0.8f + 0.5f * rnd(); p.r0 = 0.12f; p.r1 = 0.55f + 0.3f * strength;
            p.rot = rnd() * 6.28f; p.vrot = rnd_signed() * 0.8f; p.strength = 0.35f * strength;
            p.kind = EXHAUST_SMOKE; p.variant = (uint8_t)(rnd() * SMOKE_TEXTURES) % SMOKE_TEXTURES;
            p.age = -0.04f - lead;
            p.car = (uint8_t)i;
            g.parts.push_back(p);
        }
    }
    psx_mod_counter_add("r4.effects.backfire", 1);
}

/* Events, once per game frame (30/s); returns that frame's duration (s). */
float game_events(const R4RaceState &c) {
    const R4RaceCamera &cam0 = c.cameras[0];
    /* game frame duration: two VBlanks usually, more when the game slows down */
    const uint32_t nvb = g.vblank ? c.vblank - g.vblank : 2u;
    /* counter jump (savestate loaded, race restarted): no velocity and no
     * event derived from the gap to the previous frame (the "lost" speed
     * passed for an impact: a shower of sparks) */
    const bool reset = g.vblank && nvb > 8u;
    const float dt_game = (float)std::min(8u, std::max(1u, nvb)) / 60.f;
    g.vblank = c.vblank;
    g_last_vblanks.store(nvb, std::memory_order_relaxed);
    if (cam0.valid) {
        if (g.cam_known)
            for (int k = 0; k < 3; ++k) g.cam_v[k] = ((float)cam0.eye[k] - g.cam_p[k]) / dt_game;
        if (std::fabs(g.cam_v[0]) + std::fabs(g.cam_v[2]) > 200.f * M) g.cam_v[0] = g.cam_v[1] = g.cam_v[2] = 0;
        for (int k = 0; k < 3; ++k) g.cam_p[k] = (float)cam0.eye[k];
        g.cam_known = true;
    }
    if (reset) g.cam_v[0] = g.cam_v[1] = g.cam_v[2] = 0;
    for (int i = 0; i < c.car_count; ++i) {
        const R4RaceCar &v = c.cars[i];
        if (g.known[i]) {
            g.vx[i] = ((float)v.x - g.px[i]) / dt_game;
            g.vy[i] = ((float)v.y - g.py[i]) / dt_game;
            g.vz[i] = ((float)v.z - g.pz[i]) / dt_game;
            if (std::fabs(g.vx[i]) + std::fabs(g.vz[i]) > 200.f * M) {   /* jump: reload */
                g.vx[i] = g.vy[i] = g.vz[i] = 0;
                g.wheel_ok[i][0] = g.wheel_ok[i][1] = false;
                g.burst[i] = Burst{};
            }
        }
        g.px[i] = (float)v.x; g.py[i] = (float)v.y; g.pz[i] = (float)v.z;
        g.known[i] = true;
        if (reset) {
            g.vx[i] = g.vy[i] = g.vz[i] = 0;
            g.wheel_ok[i][0] = g.wheel_ok[i][1] = false;
            g.burst[i] = Burst{};
            g.prev_rpm[i] = v.rpm;
            g.spark_seen[i] = (int)c.frame;
            continue;
        }
        const float kmh = v.speed / 10.f;   /* +0x1D8 is about 10 x km/h (checked against the speedometer) */
        if (i == 0) {
            g.measured_kmh = kmh;
            g.computed_kmh = std::sqrt(g.vx[0] * g.vx[0] + g.vz[0] * g.vz[0]) / M * 3.6f;
        }
        const bool visible = cam0.valid && seen_from_behind(cam0, v, i == 0 ? 15.f : 25.f, 70.f);

        /* backfires */
        if (v.rpm > g.max_rpm[i]) g.max_rpm[i] = v.rpm;
        const int32_t rmax = g.max_rpm[i];
        if (visible) {
            if (v.since_shift == 0 && v.shift_dir > 0) {                 /* upshift: strong + echo */
                backfire(i, v, 1.f, dt_game);
                g.burst[i] = {1 + (int)(rnd() * 2), 0.09f + 0.06f * rnd(), 0.55f};
            } else if (rmax > 3000 && g.prev_rpm[i] > rmax * 7 / 10 && kmh > 60.f &&
                       (g.prev_rpm[i] - v.rpm) * 16 > g.prev_rpm[i] && g.burst[i].remaining == 0) {
                g.burst[i] = {3 + (int)(rnd() * 3), 0.f, 0.6f};            /* lift-off: pops */
            } else if (rmax > 3000 && v.rpm * 50 >= rmax * 49 && rnd() < 0.2f) {
                backfire(i, v, 0.45f, dt_game);                             /* rev limiter */
            }
        }
        g.prev_rpm[i] = v.rpm;

        /* drift: smoke strength (12 degrees and more, above 40 km/h) */
        const float slip = std::fabs((float)v.slip) * 360.f / 4096.f;
        const bool drift = slip > 12.f && kmh > 40.f && cam0.valid &&
                           seen_from_behind(cam0, v, 30.f, 80.f);
        g.drift_strength[i] = drift ? std::fmin(1.f, (slip - 12.f) / 22.f) : 0.f;
        if (g_lab_smoke.load(std::memory_order_relaxed) == i + 1) g.drift_strength[i] = 1.f;
        if (g.drift_strength[i] <= 0.f) g.wheel_ok[i][0] = g.wheel_ok[i][1] = false;

        /* sparks: on an impact, a shower along the flank */
        if (v.since_impact == 0 && g.spark_seen[i] != (int)c.frame) {
            g.spark_seen[i] = (int)c.frame;
            const int n = 14 + (int)std::fmin(26.f, v.impact_loss / 40.f);
            for (int k = 0; k < n; ++k) {
                Particle p{};
                const float side = rnd() < 0.5f ? -0.9f : 0.9f;
                car_to_world(v, side, GROUND + 0.2f, MIDDLE + rnd_signed(), p.x, p.y, p.z);
                p.vx = g.vx[i] * 0.6f + rnd_signed() * 4.f * M;
                p.vz = g.vz[i] * 0.6f + rnd_signed() * 4.f * M;
                p.vy = -(1.f + rnd() * 3.f) * M;
                p.life = 0.25f + rnd() * 0.35f;
                p.age = 0.001f - dt_game;   /* from this frame's car (see backfire) */
                p.kind = SPARK;
                p.car = (uint8_t)i;
                g.parts.push_back(p);
            }
        }
    }
    return dt_game;
}

/* Bursts and drift smoke, every game frame. */
void emit_continuous(const R4RaceState &c, float dt) {
    for (int i = 0; i < c.car_count; ++i) {
        const R4RaceCar &v = c.cars[i];
        Burst &s = g.burst[i];
        if (s.remaining > 0) {
            s.wait -= dt;
            if (s.wait <= 0.f) {
                backfire(i, v, s.strength * (0.7f + 0.5f * rnd()), dt);
                --s.remaining;
                s.wait = 0.06f + 0.11f * rnd();
            }
        }
        const float f = g.drift_strength[i];
        if (f <= 0.f) continue;
        /* smoke: emitted along the path of each rear wheel, one point every ~12 cm */
        for (int r = 0; r < 2; ++r) {
            const int side = r ? 1 : -1;
            float x, y, z;
            car_to_world(v, side * WHEEL_SIDE, GROUND + 0.05f, WHEEL_BACK, x, y, z);
            if (!g.wheel_ok[i][r]) {
                g.wheel_x[i][r] = x; g.wheel_y[i][r] = y; g.wheel_z[i][r] = z;
                g.wheel_ok[i][r] = true;
            }
            const float dx = x - g.wheel_x[i][r], dy = y - g.wheel_y[i][r], dz = z - g.wheel_z[i][r];
            const float dist = std::sqrt(dx * dx + dz * dz) / M;
            int n = (int)(dist / 0.12f);
            const int n_time = (int)(dt * (40.f + 60.f * f) + rnd());   /* even with no relative motion */
            n = std::min(40, std::max(n, n_time));
            const float lat = side * (0.6f + 0.8f * f) * M;   /* pushed outwards */
            const float cx = std::cos(v.heading * K_ANGLE), sx = std::sin(v.heading * K_ANGLE);
            for (int k = 0; k < n; ++k) {
                const float t = (k + rnd()) / std::max(1, n);
                Particle p{};
                p.x = g.wheel_x[i][r] + dx * t + rnd_signed() * 0.1f * M;
                p.y = g.wheel_y[i][r] + dy * t;
                p.z = g.wheel_z[i][r] + dz * t + rnd_signed() * 0.1f * M;
                /* dragged along by the tyre and the wake, then slowed by the
                 * air: the plume stays near the wheels and stretches out
                 * behind (93% of the car's velocity) */
                p.vx = g.vx[i] * 0.93f + cx * lat + rnd_signed() * 0.6f * M;
                p.vz = g.vz[i] * 0.93f - sx * lat + rnd_signed() * 0.6f * M;
                p.vy = -(0.15f + 0.35f * rnd()) * M;
                p.life = 2.0f + 1.4f * rnd();
                p.r0 = 0.3f + 0.15f * f; p.r1 = 1.3f + 1.2f * f + 0.4f * rnd();
                p.rot = rnd() * 6.28f; p.vrot = rnd_signed() * 0.7f;
                p.strength = 0.55f + 0.45f * f; p.phase = rnd() * 6.28f;
                p.kind = SMOKE; p.variant = (uint8_t)(rnd() * SMOKE_TEXTURES) % SMOKE_TEXTURES;
                p.age = -t * dt;   /* spread over the elapsed interval */
                p.car = (uint8_t)i;
                g.parts.push_back(p);
            }
            g.wheel_x[i][r] = x; g.wheel_y[i][r] = y; g.wheel_z[i][r] = z;
        }
    }
    constexpr size_t MAX = 3000;
    if (g.parts.size() > MAX) g.parts.erase(g.parts.begin(), g.parts.begin() + (g.parts.size() - MAX));
}

void advance(float dt) {
    for (Particle &p : g.parts) {
        const float a0 = p.age;
        p.age += dt;
        if (p.age <= 0.f) continue;
        const float h = a0 < 0.f ? p.age : dt;
        switch (p.kind) {
        case SMOKE: {
            /* first along the ground (spreading), then buoyancy and swirls */
            const float drag = std::exp(-0.75f * h);
            p.vx *= drag; p.vz *= drag;
            const float rise = p.age > 0.25f ? 0.55f : 0.08f;
            p.vy = p.vy * std::exp(-0.9f * h) - rise * M * h;
            p.vx += std::sin(p.age * 2.3f + p.phase) * 0.35f * M * h;
            p.vz += std::cos(p.age * 1.9f + p.phase * 1.3f) * 0.35f * M * h;
            break;
        }
        case EXHAUST_SMOKE: {
            const float drag = std::exp(-2.5f * h);
            p.vx *= drag; p.vz *= drag; p.vy -= 0.4f * M * h;
            break;
        }
        case FIRE: {
            const float drag = std::exp(-6.f * h);
            p.vx *= drag; p.vz *= drag; p.vy *= drag; p.vy -= 1.2f * M * h;   /* rising flame */
            p.jx *= drag; p.jy *= drag; p.jz *= drag;
            break;
        }
        case EMBER: case SPARK: {
            const float drag = std::exp(-1.2f * h);
            p.vx *= drag; p.vz *= drag;
            p.vy += 9.8f * M * h;   /* gravity (y down) */
            break;
        }
        }
        p.x += p.vx * h; p.y += p.vy * h; p.z += p.vz * h;
        p.rot += p.vrot * h;
    }
    size_t j = 0;
    for (size_t i = 0; i < g.parts.size(); ++i)
        if (g.parts[i].age < g.parts[i].life) g.parts[j++] = g.parts[i];
    g.parts.resize(j);
}

/* ---------------------------------------------------------------- drawing */

/* rotated textured quad, half-axes (rx, ry), optionally stretched along `dir` (unit) */
void quad(ImDrawList *dl, unsigned tex, ImVec2 c, float rx, float ry, float rot, ImU32 col,
          ImVec2 dir = ImVec2(0, 0), float stretch = 0.f) {
    ImVec2 ax(std::cos(rot), std::sin(rot)), ay(-ax.y, ax.x);
    if (stretch > 0.f) {   /* long axis along the motion, shifted backwards (trail) */
        ax = dir; ay = ImVec2(-dir.y, dir.x);
        c = ImVec2(c.x - dir.x * rx * stretch * 0.5f, c.y - dir.y * rx * stretch * 0.5f);
        rx *= 1.f + stretch;
    }
    const ImVec2 p1(c.x - ax.x * rx - ay.x * ry, c.y - ax.y * rx - ay.y * ry);
    const ImVec2 p2(c.x + ax.x * rx - ay.x * ry, c.y + ax.y * rx - ay.y * ry);
    const ImVec2 p3(c.x + ax.x * rx + ay.x * ry, c.y + ax.y * rx + ay.y * ry);
    const ImVec2 p4(c.x - ax.x * rx + ay.x * ry, c.y - ax.y * rx + ay.y * ry);
    dl->AddImageQuad(ImTextureRef((ImTextureID)tex), p1, p2, p3, p4, ImVec2(0, 0), ImVec2(1, 0),
                     ImVec2(1, 1), ImVec2(0, 1), col);
}

ImU32 rgba(float r, float g_, float b, float a) {
    auto q = [](float v) { return (int)(std::fmin(1.f, std::fmax(0.f, v)) * 255.f + 0.5f); };
    return IM_COL32(q(r), q(g_), q(b), q(a));
}

/* fire colour by temperature (1: bluish white, 0: dark red) */
ImU32 fire_color(float heat, float a) {
    const float c = std::fmin(1.f, std::fmax(0.f, heat));
    const float r = 1.f, gr = 0.25f + 0.7f * c, b = 0.05f + 0.75f * c * c * c;
    return rgba(r, gr, b, a);
}

/* Without a depth test, a particle hidden by its own car's body (sparks on
 * the far flank, smoke or embers behind a car seen from the front) was drawn
 * on top. The body is approximated by a slightly inset box (car frame,
 * meters): the particle is dropped if the eye -> particle segment crosses it
 * and the particle is not inside. */
bool hidden_by_car(const R4RaceState &c, int view, const Particle &p) {
    if (p.car >= c.car_count) return false;
    const R4RaceCar &v = c.cars[p.car];
    const float co = std::cos(v.heading * K_ANGLE), si = std::sin(v.heading * K_ANGLE);
    auto local = [&](float x, float y, float z, float l[3]) {
        const float dx = (x - (float)v.x) / M, dz = (z - (float)v.z) / M;
        l[0] = co * dx - si * dz; l[1] = -(y - (float)v.y) / M; l[2] = si * dx + co * dz;
    };
    float a[3], b[3];
    local((float)c.cameras[view].eye[0], (float)c.cameras[view].eye[1], (float)c.cameras[view].eye[2], a);
    local(p.x, p.y, p.z, b);
    static constexpr float bmin[3] = {-0.8f, -0.45f, -0.8f}, bmax[3] = {0.8f, 0.55f, 3.2f};
    bool inside = true;
    for (int k = 0; k < 3; ++k) inside = inside && b[k] > bmin[k] && b[k] < bmax[k];
    if (inside) return false;
    float t0 = 0.f, t1 = 1.f;
    for (int k = 0; k < 3; ++k) {
        const float d = b[k] - a[k];
        if (std::fabs(d) < 1e-6f) {
            if (a[k] < bmin[k] || a[k] > bmax[k]) return false;
            continue;
        }
        float ta = (bmin[k] - a[k]) / d, tb = (bmax[k] - a[k]) / d;
        if (ta > tb) std::swap(ta, tb);
        t0 = std::fmax(t0, ta); t1 = std::fmin(t1, tb);
        if (t0 > t1) return false;
    }
    return true;
}

void draw_view(ImDrawList *dl, const R4RaceState &c, int view, const Screen &e) {
    const float cap = e.k * 240.f * 0.28f;   /* no particle larger than 28% of the height */
    struct Sorted { float depth; const Particle *p; ImVec2 s; float r; };
    std::vector<Sorted> smokes, fires;
    smokes.reserve(g.parts.size());
    for (const Particle &p : g.parts) {
        if (p.age <= 0.f || hidden_by_car(c, view, p)) continue;
        ImVec2 s; float depth;
        if (!project(c, view, e, p.x, p.y, p.z, s, depth)) continue;
        const float t = p.age / p.life;
        if (p.kind == SMOKE || p.kind == EXHAUST_SMOKE) {
            const float rm = p.r0 + (p.r1 - p.r0) * (1.f - std::exp(-p.age * (p.kind == SMOKE ? 2.2f : 1.3f)));
            float r = screen_radius(c, view, e, rm, depth);
            if (r < 1.f) continue;
            if (r > cap) r = cap;
            smokes.push_back({depth, &p, s, r});
        } else if (p.kind == FIRE) {
            const float rm = p.r0 + (p.r1 - p.r0) * t;
            fires.push_back({depth, &p, s, std::fmin(cap, screen_radius(c, view, e, rm, depth))});
        }
    }
    /* smoke (normal blending), farthest to nearest */
    std::sort(smokes.begin(), smokes.end(), [](const Sorted &a, const Sorted &b) { return a.depth > b.depth; });
    const float amb = g.ambient;
    for (const Sorted &f : smokes) {
        const Particle &p = *f.p;
        const float t = p.age / p.life;
        float a;
        float tint;
        if (p.kind == SMOKE) {
            a = 0.5f * p.strength * std::fmin(1.f, p.age / 0.025f) * std::pow(1.f - t, 1.5f);
            tint = 0.62f * amb + 0.3f;   /* lit by the place, never lost in the dark */
        } else {
            a = p.strength * std::fmin(1.f, p.age / 0.05f) * (1.f - t);
            tint = 0.32f * amb + 0.05f;
        }
        /* young: flat and spread on the ground; then rounder as it rises */
        const float flat = 0.55f + 0.45f * std::fmin(1.f, p.age / 0.7f);
        /* too close to the camera: fades instead of covering everything */
        if (f.r >= cap) a *= 0.5f;
        if (a < 0.004f) continue;
        quad(dl, g.tex_smoke[p.variant], f.s, f.r, f.r * flat, p.rot,
             rgba(tint, tint, tint * 1.03f, a));
    }

    /* fire, embers, sparks: additive blending */
    dl->AddCallback(additive_blend, nullptr);
    for (const Sorted &f : fires) {
        const Particle &p = *f.p;
        const float t = p.age / p.life;
        if (p.phase < 0.f) {   /* core: white-yellow flash, orange halo */
            quad(dl, g.tex_fire, f.s, f.r * 2.2f, f.r * 2.2f, p.rot, fire_color(0.3f, 0.25f * p.strength * (1.f - t)));
            quad(dl, g.tex_fire, f.s, f.r, f.r, p.rot, fire_color(1.f, (1.f - t) * (0.6f + 0.4f * p.strength)));
            continue;
        }
        /* tongue: stretched along its jet (relative to the car), over ~45 ms
         * of travel, never more than the distance covered since the tailpipe
         * (otherwise a newborn tongue's trail reached ahead of the pipe, over
         * the boot) */
        ImVec2 s2; float d2;
        ImVec2 dir(0, 0); float stretch = 0.f;
        const float tr = std::fmin(0.045f, p.age);
        if (project(c, view, e, p.x - p.jx * tr, p.y - p.jy * tr, p.z - p.jz * tr, s2, d2)) {
            const ImVec2 d(f.s.x - s2.x, f.s.y - s2.y);
            const float n = std::sqrt(d.x * d.x + d.y * d.y);
            if (n > 1.f) { dir = ImVec2(d.x / n, d.y / n); stretch = std::fmin(6.f, n / std::fmax(1.f, f.r)); }
        }
        const float a = std::fmin(1.f, p.age / 0.012f) * (1.f - t) * (0.45f + 0.4f * p.strength);
        quad(dl, g.tex_fire, f.s, f.r, f.r * 0.7f, p.rot, fire_color(1.f - t * 1.3f, a), dir, stretch);
    }
    for (const Particle &p : g.parts) {
        if ((p.kind != EMBER && p.kind != SPARK) || p.age <= 0.f || hidden_by_car(c, view, p)) continue;
        ImVec2 a, b; float da, db;
        /* trail: the last 30 ms of path as seen by the camera (which follows
         * the car: without subtracting its own motion, every ember drew a
         * streak over a meter long at high speed) */
        const float tr = std::fmin(0.03f, p.age);
        const float *cv = view == 0 ? g.cam_v : nullptr;
        const float rx = p.vx - (cv ? cv[0] : 0.f), ry = p.vy - (cv ? cv[1] : 0.f), rz = p.vz - (cv ? cv[2] : 0.f);
        if (!project(c, view, e, p.x, p.y, p.z, a, da) ||
            !project(c, view, e, p.x - rx * tr, p.y - ry * tr, p.z - rz * tr, b, db))
            continue;
        const float t = p.age / p.life;
        const float w = std::fmax(1.f, e.k * (p.kind == EMBER ? 0.35f : 0.55f));
        const ImU32 col = p.kind == EMBER ? fire_color(0.75f - 0.6f * t, 1.f - t)
                                          : rgba(1.f, 0.9f, 0.6f, 1.f - t);
        dl->AddLine(b, a, col, w);
        quad(dl, g.tex_fire, a, w * 2.5f, w * 2.5f, 0.f, p.kind == EMBER ? fire_color(0.5f, 0.5f * (1.f - t))
                                                                         : rgba(1.f, 0.85f, 0.5f, 0.5f * (1.f - t)));
    }
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);

    if (g.debug) {   /* procedural textures, top left */
        for (int k = 0; k < SMOKE_TEXTURES; ++k)
            quad(dl, g.tex_smoke[k], ImVec2(80.f + k * 150.f, 260.f), 70.f, 70.f, 0.f, IM_COL32(255, 255, 255, 255));
        quad(dl, g.tex_fire, ImVec2(680.f, 260.f), 70.f, 70.f, 0.f, IM_COL32(255, 160, 60, 255));
    }
    if (g.debug)
        /* alignment, every car: axis from -3 m (red) to +3 m (green) every
         * half meter at origin height, vertical through the origin
         * (-1..+1 m, blue); origin in white, tailpipes in yellow */
        for (int i = 0; i < c.car_count; ++i) {
            const R4RaceCar &v = c.cars[i];
            ImVec2 p; float d;
            if (project(c, view, e, (float)v.x, (float)v.y, (float)v.z, p, d))
                dl->AddCircle(p, 7, IM_COL32(255, 255, 255, 255), 12, 2);
            for (int fwd = -6; fwd <= 6; ++fwd) {
                float x, y, z;
                car_to_world(v, 0.f, 0.f, fwd * 0.5f, x, y, z);
                if (!project(c, view, e, x, y, z, p, d)) continue;
                dl->AddCircleFilled(p, fwd % 2 ? 3.f : 5.f, IM_COL32(127 - fwd * 21, 127 + fwd * 21, 0, 255));
            }
            for (int h = -4; h <= 4; ++h) {
                float x, y, z;
                car_to_world(v, 0.f, h * 0.25f, 0.f, x, y, z);
                if (project(c, view, e, x, y, z, p, d)) dl->AddCircleFilled(p, 3.f, IM_COL32(60, 120, 255, 255));
            }
            for (int side = -1; side <= 1; side += 2) {
                float x, y, z;
                car_to_world(v, side * EXHAUST_SIDE, EXHAUST_UP, EXHAUST_BACK, x, y, z);
                if (project(c, view, e, x, y, z, p, d))
                    dl->AddRect(ImVec2(p.x - 4, p.y - 4), ImVec2(p.x + 4, p.y + 4), IM_COL32(255, 230, 0, 255), 0.f, 0, 2.f);
            }
        }
}

void body_reflections_off() { psx_host_set_post_uniform("u_vo", 0.f, 0.f, 0.f, 0.f); }

/* Body reflections ("reflections" pass of lighting.pfx): the player's car in
 * camera space (origin, right / up / forward axes), view 0's projection and
 * the dominant light `d` (world direction towards the light). Off in the
 * cockpit or bumper view (car not drawn) and when the car is far away. */
void body_reflections(const R4RaceState &c, const float d[3], float lw, float m) {
    const R4RaceCamera &k = c.cameras[0];
    const R4RaceCar &v = c.cars[0];
    const float dx = (float)v.x - k.eye[0], dy = (float)v.y - k.eye[1], dz = (float)v.z - k.eye[2];
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float heading = v.heading * K_ANGLE, s = std::sin(heading), co = std::cos(heading);
    if (c.car_count < 1 || k.h <= 0 || dist > 40.f * M) { body_reflections_off(); return; }
    if (dist < 1.5f * M &&
        std::sin(k.yaw * K_ANGLE) * s + std::cos(k.yaw * K_ANGLE) * co > 0.8f) { body_reflections_off(); return; }
    float o[3];
    const float w[3] = {dx / 4.f, dy / 4.f, dz / 4.f};
    /* no known dominant light: vertical direction (highlight off, w = 0) */
    const bool light_ok = d[0] != 0.f || d[1] != 0.f || d[2] != 0.f;
    const float axes[4][3] = {{co, 0.f, -s}, {0.f, -1.f, 0.f}, {s, 0.f, co},
                              {light_ok ? d[0] : 0.f, light_ok ? d[1] : -1.f, light_ok ? d[2] : 0.f}};
    float a[4][3];
    for (int i = 0; i < 3; ++i) {
        o[i] = k.matrix[i][0] * w[0] + k.matrix[i][1] * w[1] + k.matrix[i][2] * w[2];
        for (int j = 0; j < 4; ++j)
            a[j][i] = k.matrix[i][0] * axes[j][0] + k.matrix[i][1] * axes[j][1] + k.matrix[i][2] * axes[j][2];
    }
    if (o[2] < 1.f) { body_reflections_off(); return; }
    psx_host_set_post_uniform("u_vo", o[0], o[1], o[2], 1.f);
    psx_host_set_post_uniform("u_vr", a[0][0], a[0][1], a[0][2], 0.f);
    psx_host_set_post_uniform("u_vu", a[1][0], a[1][1], a[1][2], 0.f);
    psx_host_set_post_uniform("u_vf", a[2][0], a[2][1], a[2][2], 0.f);
    psx_host_set_post_uniform("u_vp", (float)k.ofx + m, (float)k.ofy, (float)k.h, lw);
    psx_host_set_post_uniform("u_vl", a[3][0], a[3][1], a[3][2],
                              light_ok ? std::fmin(1.f, std::fmax(0.25f, g.ambient)) : 0.f);
}

/* Live values of the lighting chain (postfx/r4_2026/lighting.pfx):
 * u_sun = sun uv, in front of the camera (0/1), strength; u_race = speed,
 * vanishing point, in a race. */
void lighting(const R4RaceState &c, bool split_screen) {
    const float m = (float)psx_mod_widescreen_view_x_margin();
    const float lw = 320.f + 2.f * m;
    const R4RaceCamera &k = c.cameras[0];
    if (split_screen || !k.valid) {
        psx_host_set_post_uniform("u_sun", 0.5f, 1.5f, 0.f, 0.f);
        psx_host_set_post_uniform("u_race", 0.f, 0.5f, 0.5f, 0.f);
        body_reflections_off();
        return;
    }
    /* direction towards the dominant light: whichever of the two points up (y down) */
    float d[3] = {c.light_dir[0], c.light_dir[1], c.light_dir[2]};
    if (d[1] > 0.f) { d[0] = -d[0]; d[1] = -d[1]; d[2] = -d[2]; }
    const float distant = 4.0e6f;
    float sx, sy, depth;
    const int in_front = (d[0] != 0.f || d[1] != 0.f || d[2] != 0.f) &&
                         r4_race_project(&c, 0, k.eye[0] + d[0] * distant, k.eye[1] + d[1] * distant,
                                         k.eye[2] + d[2] * distant, &sx, &sy, &depth);
    const float su = (sx + m) / lw, sv = 1.f - sy / 240.f;
    /* far off screen: no rays and no flare (passes skipped) */
    if (in_front && su > -0.6f && su < 1.6f && sv > -0.6f && sv < 1.6f)
        psx_host_set_post_uniform("u_sun", su, sv, 1.f, 1.f);
    else psx_host_set_post_uniform("u_sun", 0.5f, 1.5f, 0.f, 0.f);
    const float kmh = (float)(int16_t)psx_mod_read_half(0x800AC0B0u + 0x296u);   /* player's speedometer */
    psx_host_set_post_uniform("u_race", std::fmin(1.f, kmh / 300.f), ((float)k.ofx + m) / lw,
                              1.f - (float)k.ofy / 240.f, 1.f);
    body_reflections(c, d, lw, m);
}

}  // namespace

/* r4.effects: flame CAR [STRENGTH] | smoke CAR|off | everywhere 0|1 | active 0|1 | debug 0|1 | delay N | measure */
static int effects_command(const char *args, char *reply, int cap) {
    char word[16] = "";
    std::sscanf(args, "%15s", word);
    int v = 0; float f = 1.f;
    if (!std::strcmp(word, "flame") && std::sscanf(args, "%*s %d %f", &v, &f) >= 1) {
        g_lab_flame.store((v + 1) | ((int)(std::fmin(1.f, std::fmax(0.f, f)) * 1000.f) << 8));
        std::snprintf(reply, (size_t)cap, "flame car %d strength %.2f", v, f);
        return 1;
    }
    if (!std::strcmp(word, "smoke")) {
        if (std::sscanf(args, "%*s %d", &v) == 1) g_lab_smoke.store(v + 1); else g_lab_smoke.store(0);
        std::snprintf(reply, (size_t)cap, "smoke %s", g_lab_smoke.load() ? "forced" : "off");
        return 1;
    }
    if (!std::strcmp(word, "everywhere") && std::sscanf(args, "%*s %d", &v) == 1) {
        g_lab_everywhere.store(v); std::snprintf(reply, (size_t)cap, "everywhere %d", v); return 1;
    }
    if (!std::strcmp(word, "active") && std::sscanf(args, "%*s %d", &v) == 1) {
        g_lab_active.store(v); std::snprintf(reply, (size_t)cap, "active %d", v); return 1;
    }
    if (!std::strcmp(word, "debug") && std::sscanf(args, "%*s %d", &v) == 1) {
        g_lab_debug.store(v); std::snprintf(reply, (size_t)cap, "debug %d", v); return 1;
    }
    if (!std::strcmp(word, "measure")) {
        std::snprintf(reply, (size_t)cap, "speedometer %.1f km/h, computed %.1f km/h, camera %.1f m/s, dt %u vb",
                      g.measured_kmh, g.computed_kmh,
                      std::sqrt(g.cam_v[0] * g.cam_v[0] + g.cam_v[2] * g.cam_v[2]) / M, g_last_vblanks.load());
        return 1;
    }
    if (!std::strcmp(word, "delay") && std::sscanf(args, "%*s %d", &v) == 1) {
        g_lab_delay.store(v); std::snprintf(reply, (size_t)cap, "delay %d", v); return 1;
    }
    std::snprintf(reply, (size_t)cap, "flame C [S] | smoke C|off | everywhere 0|1 | active 0|1 | debug 0|1 | delay N | measure");
    return 0;
}

bool r4_car_effects_active() { return s_enabled.load(std::memory_order_relaxed); }

void r4_car_effects_off_race() {
    psx_host_set_post_uniform("u_sun", 0.5f, 1.5f, 0.f, 0.f);
    psx_host_set_post_uniform("u_race", 0.f, 0.5f, 0.5f, 0.f);
    body_reflections_off();
}

void r4_car_effects_draw(ImDrawList *dl, int W, int H, bool split_screen) {
    if (!r4_car_effects_active()) return;
    if (!g.env_read) {
        g.env_read = true;
        const char *e = std::getenv("PSX_R4_EFFECTS");
        g.active = !(e && e[0] == '0');
        const char *d = std::getenv("PSX_R4_EFFECTS_DEBUG");
        g.debug = d && d[0] == '1';
        const char *r = std::getenv("PSX_R4_EFFECTS_DELAY");
        if (r && r[0] >= '0' && r[0] <= '3') g.delay = r[0] - '0';
        load_gl();
    }
    if (const int x = g_lab_active.exchange(-1); x >= 0) {
        g.active = x != 0;
        if (!g.active) { g.parts.clear(); r4_car_effects_off_race(); }
    }
    if (!g.active) return;
    R4RaceState c;
    psx_mod_counter_add("r4.effects.draw", 1);
    if (const int d = g_lab_debug.exchange(-1); d >= 0) g.debug = d != 0;
    if (const int r = g_lab_delay.exchange(-1); r >= 0) g.delay = r;
    if (!r4_race_state_read_delayed(&c, g.delay) || !c.in_race) {
        g.parts.clear();
        for (int32_t &r : g.max_rpm) r = 0;
        for (Burst &s : g.burst) s = Burst{};
        r4_car_effects_off_race();
        return;
    }
    if (!textures_ready()) return;

    measure_ambient(W, H);
    /* Particles advance at the game's pace, like the cars and the camera they
     * are drawn with (the state of one game frame, held for two presented
     * frames). Advanced on every presented frame, those moving at the car's
     * speed got up to a game frame ahead of it (2.5 m at full speed): flames
     * and embers crept onto the boot instead of leaving the tailpipes. */
    if (c.frame != g.frame) {
        const float dt_game = game_events(c);
        g.frame = c.frame;
        emit_continuous(c, dt_game);
        advance(dt_game);
    }
    if (const int f = g_lab_flame.exchange(0)) {   /* after the advance: from the car on screen */
        const int i = (f & 0xFF) - 1;
        if (i >= 0 && i < c.car_count) backfire(i, c.cars[i], (float)(f >> 8) / 1000.f, 0.f);
    }
    lighting(c, split_screen);

    const Screen e = screen_map(W, H);
    if (!split_screen) {
        draw_view(dl, c, 0, e);
        return;
    }
    /* split screen: views 1 (top) and 2 (bottom), each clipped to its half */
    for (int view = 1; view <= 2; ++view) {
        dl->PushClipRect(ImVec2(0, view == 1 ? 0 : H * 0.5f), ImVec2((float)W, view == 1 ? H * 0.5f : (float)H));
        draw_view(dl, c, view, e);
        dl->PopClipRect();
    }
}

static void r4_car_effects_activate(void) {
    s_enabled.store(true, std::memory_order_relaxed);
    r4_race_hud_bind();
}

/* Host-only: a plan without this plugin (or a netplay match) starts with the
 * effects off and the lighting chain's live values neutral. */
static void r4_car_effects_session_reset(void) {
    s_enabled.store(false, std::memory_order_relaxed);
    r4_car_effects_off_race();
}

PSX_MOD_CONSTRUCTOR(r4_register_car_effects) {
    (void)psx_mod_register_activation_plugin(PLUGIN_ID, r4_car_effects_activate);
    (void)psx_mod_register_session_reset_plugin(PLUGIN_ID, r4_car_effects_session_reset);
    (void)psx_mod_register_command("r4.effects", effects_command);
}
