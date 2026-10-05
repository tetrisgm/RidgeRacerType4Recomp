/* The real R4 JogCon compatibility hook against a mock guest RAM/API. */
#include <stdio.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"

#define PAD_BASE 0x800F3BE8u
#define PAD_TYPE (PAD_BASE + 1u)
#define PAD_MODE (PAD_BASE + 16u)
#define INPUT_UPDATE 0x8002961Cu

static uint8_t s_ram[2u * 1024u * 1024u];
static PSXModFunctionEntryCallback s_hook;
static uint32_t s_hook_address;
static int s_failures;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); s_failures++; } } while (0)

static uint8_t *at(uint32_t a) { return &s_ram[a & 0x1FFFFFu]; }
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; memcpy(&v, at(a), sizeof v); return v; }
void psx_mod_write_word(uint32_t a, uint32_t v) { memcpy(at(a), &v, sizeof v); }
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                           PSXModFunctionEntryCallback cb) {
    if (!id || strcmp(id, "r4.jogcon-input") != 0 || !cb) return 0;
    s_hook = cb;
    s_hook_address = address;
    return 1;
}

static void set_pad(uint8_t type, uint32_t mode) {
    *at(PAD_TYPE) = type;
    psx_mod_write_word(PAD_MODE, mode);
    /* Non-mode fields must survive the compatibility write. */
    psx_mod_write_word(PAD_BASE + 0x3Cu, 0x12345678u);
    psx_mod_write_word(PAD_BASE + 0x40u, 0x9ABCDEF0u);
}

int main(void) {
    CPUState cpu;
    memset(&cpu, 0, sizeof cpu);
    CHECK(s_hook && s_hook_address == INPUT_UPDATE, "registered on R4 input updater");
    if (!s_hook) return 1;

    set_pad(0xE3u, 2u);
    s_hook(&cpu, INPUT_UPDATE);
    CHECK(psx_mod_read_byte(PAD_TYPE) == 0xE3u, "keeps guest JogCon ID E3");
    CHECK(psx_mod_read_word(PAD_MODE) == 1u, "maps R4 JogCon mode 2 to its analog input path");
    CHECK(psx_mod_read_word(PAD_BASE + 0x3Cu) == 0x12345678u &&
          psx_mod_read_word(PAD_BASE + 0x40u) == 0x9ABCDEF0u,
          "preserves wheel steering and neighboring decoded fields");
    s_hook(&cpu, INPUT_UPDATE);
    CHECK(psx_mod_read_word(PAD_MODE) == 1u, "mode mapping is idempotent");

    set_pad(0x73u, 1u);
    s_hook(&cpu, INPUT_UPDATE);
    CHECK(psx_mod_read_byte(PAD_TYPE) == 0x73u && psx_mod_read_word(PAD_MODE) == 1u,
          "native DualShock analog mode remains unchanged");
    set_pad(0x41u, 0u);
    s_hook(&cpu, INPUT_UPDATE);
    CHECK(psx_mod_read_word(PAD_MODE) == 0u, "digital mode remains unchanged");
    set_pad(0xE3u, 2u);
    s_hook(&cpu, INPUT_UPDATE + 4u);
    CHECK(psx_mod_read_word(PAD_MODE) == 2u, "ignores other call sites");

    if (s_failures) return 1;
    puts("R4 JogCon compatibility tests passed");
    return 0;
}
