/* The real R4 Modern rumble hook against a mock guest RAM / mod API, with
 * R4's own DualShock and legacy vibration tables (US EXE). */
#include <stdio.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_modern_rumble.h"

#define PAD0 0x800F3BE8u
#define HOOK 0x8002961Cu

static uint8_t s_ram[2u * 1024u * 1024u];
static PSXModFunctionEntryCallback s_hook;
static uint32_t s_hook_address;
static int s_failures, s_calls[4];
static uint32_t s_small[4], s_large[4];

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); s_failures++; } } while (0)

static uint8_t *at(uint32_t a) { return &s_ram[a & 0x1FFFFFu]; }
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
uint16_t psx_mod_read_half(uint32_t a) { uint16_t v; memcpy(&v, at(a), 2); return v; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; memcpy(&v, at(a), 4); return v; }
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                           PSXModFunctionEntryCallback cb) {
    if (!id || strcmp(id, "r4.modern-rumble") != 0 || !cb) return 0;
    s_hook = cb; s_hook_address = address;
    return 1;
}
int psx_mod_set_host_rumble(uint32_t player, uint32_t small, uint32_t large) {
    if (player >= 4) return 0;
    s_calls[player]++; s_small[player] = small; s_large[player] = large;
    return 1;
}

static void put16(uint32_t a, uint16_t v) { memcpy(at(a), &v, 2); }
static void put32(uint32_t a, uint32_t v) { memcpy(at(a), &v, 4); }

/* Pattern entries {small, large, end frame}, closed by end -1. */
static void put_pattern(uint32_t table, int index, uint32_t addr,
                        const int (*e)[3], int n) {
    put32(table + (uint32_t)index * 4u, addr);
    for (int k = 0; k < n; ++k, addr += 4u) {
        *at(addr) = (uint8_t)e[k][0]; *at(addr + 1u) = (uint8_t)e[k][1];
        put16(addr + 2u, (uint16_t)(int16_t)e[k][2]);
    }
}

static void tables(void) {
    /* R4 US: A[0] (DualShock) and A[3] (legacy for id 1); B[0] and B[3]. */
    static const int a0[][3] = { {0,170,4}, {1,0,8}, {0,50,12}, {1,0,13}, {0,0,15}, {255,255,-1} };
    static const int a3[][3] = { {1,0,1}, {0,0,4}, {1,0,6}, {0,0,10}, {255,255,-1} };
    static const int b0[][3] = { {0,150,6}, {1,64,8}, {1,0,10}, {255,255,-1} };
    static const int b3[][3] = { {0,255,4}, {255,255,-1} };
    put_pattern(R4_RUMBLE_TABLE_A, 0, 0x800A0084u, a0, 6);
    put_pattern(R4_RUMBLE_TABLE_A, 3, 0x800A00F4u, a3, 5);
    put_pattern(R4_RUMBLE_TABLE_B, 0, 0x800A0044u, b0, 4);
    put_pattern(R4_RUMBLE_TABLE_B, 3, 0x800A007Cu, b3, 2);
}

static void state(uint32_t port, int id_a, int clk_a, int id_b, int clk_b) {
    const uint32_t st = R4_RUMBLE_STATE + port * R4_RUMBLE_STATE_STRIDE;
    put16(st + 4u, (uint16_t)id_a); put16(st + 8u, (uint16_t)clk_a);
    put16(st + 6u, (uint16_t)id_b); put16(st + 10u, (uint16_t)clk_b);
}

static void run(void) { memset(s_calls, 0, sizeof s_calls); s_hook(NULL, HOOK); }

int main(void) {
    CHECK(s_hook && s_hook_address == HOOK, "hooked on R4's input update");
    if (!s_hook) return 1;
    tables();
    *at(PAD0 + 1u) = 0x23u; *at(PAD0 + 0x5Cu + 1u) = 0x23u;

    state(0, 0, 0, 0, 0); run();
    CHECK(s_calls[0] == 1 && s_small[0] == 0 && s_large[0] == 0, "idle: motors off");

    state(0, 1, 0, 0, 0); run();
    CHECK(s_small[0] == 0 && s_large[0] == 170, "pattern A id 1, clock 0: large 170");
    state(0, 1, 5, 0, 0); run();
    CHECK(s_small[0] == 1 && s_large[0] == 0, "clock 5: small on");
    state(0, 1, 9, 0, 0); run();
    CHECK(s_small[0] == 0 && s_large[0] == 50, "clock 9: large 50");
    state(0, 1, 40, 0, 0); run();
    CHECK(s_small[0] == 0 && s_large[0] == 0, "past the pattern: off");

    state(0, 1, 0, 1, 6); run();
    CHECK(s_small[0] == 1 && s_large[0] == 64, "channel B wins over A");

    put16(R4_RUMBLE_OFF_FLAGS + 2u, 1);
    state(0, 1, 0, 0, 0); run();
    CHECK(s_small[0] == 0 && s_large[0] == 0, "R4's vibration off setting honoured");
    put16(R4_RUMBLE_OFF_FLAGS + 2u, 0);

    put16(PAD0 + 0x1Cu, 1); run();
    CHECK(s_calls[0] == 0, "DualShock mode: R4 rumbles over SIO itself, skipped");
    put16(PAD0 + 0x1Cu, 0);
    *at(PAD0 + 1u) = 0x73u; run();
    CHECK(s_calls[0] == 0, "non-NeGcon pad skipped (Classic)");
    *at(PAD0 + 1u) = 0x23u;

    state(1, 0, 0, 1, 0); run();
    CHECK(s_calls[1] == 1 && s_large[1] == 150, "port 2 has its own state");
    state(0, 9, 0, 0, 0); run();
    CHECK(s_small[0] == 0 && s_large[0] == 0, "out-of-range pattern id ignored");

    if (s_failures) { fprintf(stderr, "%d failure(s)\n", s_failures); return 1; }
    puts("R4 modern rumble tests passed");
    return 0;
}
