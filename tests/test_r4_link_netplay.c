#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[2u * 1024u * 1024u];
static uint8_t rollback_ram[sizeof ram];
static int seat_count;
static int registrations;
static uint8_t mod_ram[68];
static uint8_t rollback_mod_ram[68];
static uint8_t gpu_dma_ram[4u * 0xB00u + 8u];

#include "../src/mods/r4_link_netplay.c"

static PsxNetPad sample_pads[4];
static int sample_pad_valid[4];

static int local_slot;
static unsigned local_views;
static uint32_t local_view[4];
int psx_netplay_seat_count(void) { return seat_count; }
int psx_netplay_local_slot(void) { return local_slot; }
void psx_netplay_present_local_view(uint32_t x, uint32_t y,
                                    uint32_t w, uint32_t h) {
    local_view[0] = x;
    local_view[1] = y;
    local_view[2] = w;
    local_view[3] = h;
    ++local_views;
}
int psx_netplay_sim_pad(int seat, PsxNetPad *out) {
    if (!out || seat < 0 || seat >= 4 || !sample_pad_valid[seat]) return 0;
    *out = sample_pads[seat];
    return 1;
}
uint8_t psx_mod_read_byte(uint32_t address) {
    if (address >= 0x9F000000u && address < 0x9F000044u)
        return mod_ram[address - 0x9F000000u];
    return ram[address & 0x1FFFFFu];
}
uint32_t psx_mod_alloc_guest_memory(uint32_t size, uint32_t alignment) {
    assert(alignment == 4);
    if (size == 4) return 0x9F000000u;
    assert(size == 64);
    return 0x9F000004u;
}
uint32_t psx_mod_alloc_gpu_dma_memory(uint32_t size, uint32_t alignment) {
    assert(size == sizeof gpu_dma_ram && alignment == 4);
    return 0x80800000u;
}
uint16_t psx_mod_read_half(uint32_t address) {
    uint16_t value;
    memcpy(&value, ram + (address & 0x1FFFFFu), sizeof value);
    return value;
}
uint32_t psx_mod_read_word(uint32_t address) {
    uint32_t value;
    if (address >= 0x80800000u &&
        address + 4u <= 0x80800000u + sizeof gpu_dma_ram) {
        memcpy(&value, gpu_dma_ram + address - 0x80800000u, sizeof value);
        return value;
    }
    memcpy(&value, ram + (address & 0x1FFFFFu), sizeof value);
    return value;
}
void psx_mod_write_byte(uint32_t address, uint8_t value) {
    if (address >= 0x9F000000u && address < 0x9F000044u) {
        mod_ram[address - 0x9F000000u] = value;
        return;
    }
    ram[address & 0x1FFFFFu] = value;
}
void psx_mod_write_half(uint32_t address, uint16_t value) {
    memcpy(ram + (address & 0x1FFFFFu), &value, sizeof value);
}
void psx_mod_write_word(uint32_t address, uint32_t value) {
    if (address >= 0x80800000u &&
        address + 4u <= 0x80800000u + sizeof gpu_dma_ram) {
        memcpy(gpu_dma_ram + address - 0x80800000u, &value, sizeof value);
        return;
    }
    memcpy(ram + (address & 0x1FFFFFu), &value, sizeof value);
}
void psx_mod_write_code_word(uint32_t address, uint32_t value) {
    psx_mod_write_word(address, value);
}
int psx_game_register_netplay_function_filter(
    uint32_t address, PSXModFunctionFilterCallback callback) {
    assert(address && callback);
    ++registrations;
    return 1;
}

int main(void)
{
    assert(registrations == 27);
    assert(setenv("PSX_R4_LINK_EXPERIMENTAL", "1", 1) == 0);
    r4_link_brake_ramps = psx_mod_alloc_guest_memory(4, 4);
    r4_link_extra_view_cameras = psx_mod_alloc_guest_memory(64, 4);
    r4_link_extra_ot = psx_mod_alloc_gpu_dma_memory(R4_LINK_OT_ALLOC_BYTES, 4);
    memset(ram, 0x5A, sizeof ram);
    psx_mod_write_word(R4_LINK_OVERLAY_BASE, 9);
    psx_mod_write_word(0x80114C38u, 0x0C024BA5u);
    psx_mod_write_word(0x801155CCu, 0x0C024AECu);
    ram[R4_LINK_MODE & 0x1FFFFFu] = 4;
    ram[(R4_LINK_MODE + 1u) & 0x1FFFFFu] = 0;
    seat_count = 1; /* a lone peer is not a link session */
    assert(!r4_link_serial_filter(0, 0x80114C28u));
    assert(psx_mod_read_word(0x80119004u) == 0x5A5A5A5Au);
    seat_count = 5;
    assert(!r4_link_serial_filter(0, 0x80114C28u));
    assert(psx_mod_read_word(0x80119004u) == 0x5A5A5A5Au);
    seat_count = 4;
    CPUState cpu = {0};
    ram[R4_LINK_MODE & 0x1FFFFFu] = 0;
    assert(r4_link_serial_filter(&cpu, 0x8003535Cu));
    assert(cpu.gpr[2] == 1);
    cpu.gpr[31] = 0x8002F8A0u;
    assert(r4_link_serial_filter(&cpu, 0x8009B088u));
    assert(cpu.gpr[2] == 0x100);
    cpu.gpr[31] = 0x8002F8A4u;
    assert(!r4_link_serial_filter(&cpu, 0x8009B088u));
    ram[R4_LINK_MODE & 0x1FFFFFu] = 4;
    assert(r4_link_serial_filter(&cpu, 0x8009B088u));
    assert(cpu.gpr[2] == 0x100);
    assert(r4_link_serial_filter(0, 0x80114AB8u));
    assert(psx_mod_read_word(0x80119008u) == 0);
    assert(r4_link_serial_filter(0, 0x80114C28u));
    cpu.gpr[4] = 0;
    cpu.gpr[31] = 0x80114BB0u;
    assert(r4_link_serial_filter(&cpu, 0x800970A0u));
    assert(cpu.gpr[2] == 1);
    cpu.gpr[31] = 0x80114BE8u;
    assert(r4_link_serial_filter(&cpu, 0x80097060u));
    assert(cpu.gpr[2] == 1);
    cpu.gpr[31] = 0x80000000u;
    assert(!r4_link_serial_filter(&cpu, 0x80097060u));
    cpu.gpr[31] = 0x80114BE8u;
    cpu.gpr[4] = 0xF1000001u;
    assert(!r4_link_serial_filter(&cpu, 0x80097060u));
    cpu.gpr[4] = 0;
    assert(psx_mod_read_word(0x80119004u) == 0);
    assert(psx_mod_read_word(0x80119050u) == 0);
    assert(psx_mod_read_word(0x801190ACu) == 0);
    assert(psx_mod_read_word(0x801190B0u) == 0);
    assert(psx_mod_read_byte(0x801190BCu) == 7);
    assert(psx_mod_read_word(0x800F3CA0u) == 0);
    assert(psx_mod_read_word(0x800ACDA8u) == 0);
    assert(psx_mod_read_word(0x800ACD98u) == 0);
    assert(psx_mod_read_byte(R4_LINK_PAUSE_LATCH) == 0);
    assert(psx_mod_read_byte(R4_LINK_MENU_DIRECTION_LATCH) == 0);
    assert(r4_link_serial_filter(0, 0x801155C4u));
    assert(setenv("PSX_R4_LINK_PROBE_COMMANDS",
                  "20000000,2000ff00,2100ff00,2200ff00", 1) == 0);
    uint32_t commands[4];
    assert(r4_link_probe_commands(commands));
    psx_mod_write_half(0x800AD6C0u, 1);
    r4_link_stage_commands(commands);
    assert(psx_mod_read_word(0x800ACDA8u) == commands[0]);
    assert(psx_mod_read_word(0x800ACDACu) == commands[1]);
    assert(psx_mod_read_word(0x800ACD98u) == commands[2]);
    assert(psx_mod_read_word(0x800ACD9Cu) == commands[3]);
    psx_mod_write_half(0x800AD6C0u, 0);
    r4_link_stage_commands(commands);
    assert(psx_mod_read_word(0x800ACD98u) == commands[0]);
    assert(psx_mod_read_word(0x800ACDA8u) == commands[2]);
    unsetenv("PSX_R4_LINK_PROBE_COMMANDS");
    psx_mod_write_half(0x800AD6C0u, 1);
    for (int i = 0; i < 4; ++i) {
        sample_pad_valid[i] = 1;
        sample_pads[i].connected = 1;
        sample_pads[i].buttons = 0xFFFFu;
    }
    sample_pads[1].buttons = 0xBFFFu; /* X */
    sample_pads[2].buttons = 0xBF7Fu; /* X + left */
    sample_pads[3].buttons = 0xBFDFu; /* X + right */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_word(0x800ACDA8u) == 0x20000000u);
    assert(psx_mod_read_word(0x800ACDACu) == 0x2000FF00u);
    assert(psx_mod_read_word(0x800ACD98u) == 0x2100FF00u);
    assert(psx_mod_read_word(0x800ACD9Cu) == 0x2200FF00u);
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    sample_pads[3].buttons &= (uint16_t)~0x0008u; /* P4 Start */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 1);
    assert(psx_mod_read_byte(0x8011901Eu) == 0);
    assert(psx_mod_read_byte(R4_LINK_PAUSE_LATCH) == 8);
    assert(psx_mod_read_word(0x800ACD9Cu) == 0x2200FF00u);
    assert(r4_link_stage_netplay_commands()); /* holding Start does not toggle */
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    sample_pads[1].buttons &= (uint16_t)~0x0008u;
    assert(r4_link_stage_netplay_commands()); /* another seat can toggle */
    assert(psx_mod_read_byte(0x8011901Du) == 1);
    sample_pads[1].buttons |= 0x0008u;
    sample_pads[3].buttons |= 0x0008u;
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    assert(psx_mod_read_byte(R4_LINK_PAUSE_LATCH) == 0);
    sample_pads[2].buttons &= (uint16_t)~0x0010u; /* P3 Up */
    sample_pads[3].buttons &= (uint16_t)~0x0040u; /* P4 Down */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 6); /* both menu directions */
    assert(psx_mod_read_byte(R4_LINK_MENU_DIRECTION_LATCH) == 0x84);
    assert(psx_mod_read_word(0x800ACD98u) == 0x2100FF00u);
    assert(psx_mod_read_word(0x800ACD9Cu) == 0x2200FF00u);
    assert(r4_link_stage_netplay_commands()); /* held directions do not repeat */
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    sample_pads[0].buttons &= (uint16_t)~0x0010u; /* new Up from P1 */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 2);
    sample_pads[0].buttons |= 0x0010u;
    sample_pads[2].buttons |= 0x0010u;
    sample_pads[3].buttons |= 0x0040u;
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    assert(psx_mod_read_byte(R4_LINK_MENU_DIRECTION_LATCH) == 0);
    sample_pads[0].buttons &= (uint16_t)~0x2000u; /* Circle is not retail confirm */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_byte(0x8011901Du) == 0);
    sample_pads[0].buttons |= 0x2000u;
    seat_count = 3;
    sample_pad_valid[3] = 0; /* no fourth remote input is required */
    assert(r4_link_stage_netplay_commands());
    assert(psx_mod_read_word(0x800ACD9Cu) == 0x20000000u);
    assert(psx_mod_read_byte(R4_LINK_PAUSE_LATCH) == 0);
    psx_mod_write_half(0x800FF838u, 3);
    assert(!r4_link_serial_filter(&cpu, 0x80035EA0u));
    assert(psx_mod_read_word(0x800AC074u) == 0x00010101u);
    assert(psx_mod_read_half(0x800FF838u) == 2);
    psx_mod_write_half(0x800AC754u, 4);
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(psx_mod_read_half(0x800AC754u) == 3);
    psx_mod_write_word(0x800ACDCCu, 0x800ADCA0u);
    psx_mod_write_word(0x800AC064u, 123u);
    psx_mod_write_word(0x80115D90u, 0x24140002u);
    psx_mod_write_word(0x80115F5Cu, 0x26100320u);
    for (unsigned i = 0; i < 6; ++i)
        psx_mod_write_word(0x801190C0u + 4u * i, 0);
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(psx_mod_read_word(0x80115D90u) == 0x24140003u);
    assert(psx_mod_read_word(0x80115F5Cu) ==
           (0x08000000u | ((0x801190C0u >> 2) & 0x03FFFFFFu)));
    assert(psx_mod_read_word(0x801190CCu) == 0x8D10FDD0u);
    assert(psx_mod_read_word(0x80800000u) == 0x00FFFFFFu);
    assert(psx_mod_read_word(0x80800AFCu) == 0x00800AF8u);
    assert(psx_mod_read_word(0x80800B00u) == 0x00FFFFFFu);
    assert(psx_mod_read_word(0x80801600u) == 0); /* other frame untouched */
    assert(psx_mod_read_word(0x80802C00u) == 123u);
    const uint32_t buf = 0x800ADCA0u;
    const uint32_t env2 = buf + 0x1670u;
    const uint32_t env3 = buf + 0x1680u;
    psx_mod_write_word(buf + 0xB6Cu, env2 & 0x00FFFFFFu);
    psx_mod_write_word(env2, (2u << 24) | ((buf + 0x70u) & 0x00FFFFFFu));
    psx_mod_write_word(env2 + 4u, 0xE3000000u);
    psx_mod_write_word(env2 + 8u, 0xE4000000u);
    assert(r4_link_extract_view_env(2));
    assert(psx_mod_read_word(buf + 0xB6Cu) == ((buf + 0x70u) & 0x00FFFFFFu));
    assert(psx_mod_read_word(r4_link_extra_ot_for(2)) == (env2 & 0x00FFFFFFu));
    psx_mod_write_word(buf + 0x166Cu, env3 & 0x00FFFFFFu);
    psx_mod_write_word(env3, (2u << 24) | ((buf + 0xB70u) & 0x00FFFFFFu));
    psx_mod_write_word(env3 + 4u, 0xE3000000u);
    psx_mod_write_word(env3 + 8u, 0xE4000000u);
    assert(r4_link_extract_view_env(3));
    assert(psx_mod_read_word(buf + 0x166Cu) == ((buf + 0xB70u) & 0x00FFFFFFu));
    psx_mod_write_word(buf + 0xB70u, 0x000ABD38u);
    const uint32_t shared = psx_mod_read_word(0x800ABD38u);
    assert(r4_link_append_ot(buf + 0x166Cu, 4));
    assert(psx_mod_read_word(buf + 0xB70u) == (env2 & 0x00FFFFFFu));
    assert((psx_mod_read_word(env2) & 0x00FFFFFFu) ==
           ((r4_link_extra_ot_for(2) + R4_LINK_OT_BYTES - 4u) & 0x00FFFFFFu));
    assert(psx_mod_read_word(r4_link_extra_ot_for(2)) ==
           (env3 & 0x00FFFFFFu));
    assert((psx_mod_read_word(env3) & 0x00FFFFFFu) ==
           ((r4_link_extra_ot_for(3) + R4_LINK_OT_BYTES - 4u) & 0x00FFFFFFu));
    assert(psx_mod_read_word(r4_link_extra_ot_for(3)) == 0x000ABD38u);
    assert(psx_mod_read_word(env2 + 4u) == (0xE3000000u | (120u << 10)));
    assert(psx_mod_read_word(env2 + 8u) ==
           (0xE4000000u | (239u << 10) | 159u));
    assert(psx_mod_read_word(env3 + 4u) ==
           (0xE3000000u | (120u << 10) | 160u));
    assert(psx_mod_read_word(env3 + 8u) ==
           (0xE4000000u | (239u << 10) | 319u));
    assert(psx_mod_read_word(0x800ABD38u) == shared);
    psx_mod_write_word(buf + 0x166Cu, (buf + 0x166Cu) & 0x00FFFFFFu);
    assert(!r4_link_append_ot(buf + 0x166Cu, 4)); /* bounded cycle */
    psx_mod_write_word(0x800FFDD8u, 0x80107348u);
    psx_mod_write_word(0x800FFDDCu, 0x80107668u);
    cpu.gpr[17] = 2;
    cpu.gpr[19] = 0x800AC6F0u; /* retail stride is wrong for P3 */
    cpu.gpr[22] = 0x80118FD8u;
    cpu.gpr[23] = 0x80118FF8u;
    assert(!r4_link_serial_filter(&cpu, 0x80115FA8u));
    assert(cpu.gpr[19] == 0x80107348u);
    assert(cpu.gpr[22] == 0x9F000004u);
    assert(cpu.gpr[23] == 0x9F000014u);
    assert(!r4_link_serial_filter(&cpu, 0x80116024u));
    assert(psx_mod_read_word(0x1F800004u) == 0x80800000u);
    seat_count = 4;
    cpu.gpr[17] = 3;
    assert(!r4_link_serial_filter(&cpu, 0x80115FA8u));
    assert(cpu.gpr[19] == 0x80107668u);
    assert(cpu.gpr[22] == 0x9F000024u);
    assert(cpu.gpr[23] == 0x9F000034u);
    assert(!r4_link_serial_filter(&cpu, 0x80116024u));
    psx_mod_write_word(0x80115D90u, 0x24140002u);
    psx_mod_write_word(0x80115F5Cu, 0x26100320u);
    for (unsigned i = 0; i < 6; ++i)
        psx_mod_write_word(0x801190C0u + 4u * i, 0);
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(psx_mod_read_word(0x80115D90u) == 0x24140004u);
    for (unsigned view = 0; view < 4; ++view) {
        R4LinkViewport before, expected, actual;
        unsigned index = view + 2u;
        cpu.gpr[17] = view;
        cpu.gpr[4] = index;
        r4_link_viewport_record(index, &before);
        psx_mod_write_word(0x800A3D7Cu + 4u * index, 290u);
        assert(!r4_link_serial_filter(&cpu, view ? 0x80115FA8u : 0x80115F84u));
        r4_link_layout_viewport(4, view, &expected);
        r4_link_viewport_record(index, &actual);
        assert(memcmp(&actual, &expected, sizeof actual) == 0);
        assert(psx_mod_read_word(0x800A3D7Cu + 4u * index) == 145u);
        assert(!r4_link_serial_filter(&cpu, 0x80116024u));
        r4_link_viewport_record(index, &actual);
        assert(memcmp(&actual, &before, sizeof actual) == 0);
        assert(psx_mod_read_word(0x800A3D7Cu + 4u * index) == 290u);
    }
    seat_count = 3;
    for (unsigned view = 0; view < 3; ++view) {
        R4LinkViewport expected, actual;
        unsigned index = view + 2u;
        cpu.gpr[17] = view;
        cpu.gpr[4] = index;
        assert(!r4_link_serial_filter(&cpu, view ? 0x80115FA8u : 0x80115F84u));
        r4_link_layout_viewport(3, view, &expected);
        r4_link_viewport_record(index, &actual);
        assert(memcmp(&actual, &expected, sizeof actual) == 0);
        /* Every seat count uses the same 160x120 quadrants. */
        assert(actual.left == (view & 1u ? 160u : 0u));
        assert(actual.top == (view >= 2u ? 120u : 0u));
        assert(actual.rect_w == 160u && actual.rect_h == 120u);
        assert(psx_mod_read_word(0x800A3D7Cu + 4u * index) == 145u);
        assert(!r4_link_serial_filter(&cpu, 0x80116024u));
    }
    seat_count = 4;
    cpu.gpr[31] = 0x80115F18u;
    cpu.gpr[4] = 145u; /* real car speed survives style-record aliasing */
    cpu.gpr[5] = 2u;
    assert(!r4_link_serial_filter(&cpu, 0x80021614u));
    assert(cpu.gpr[4] == 145u && cpu.gpr[5] == 0u);
    cpu.gpr[31] = 0x80115F24u;
    cpu.gpr[4] = 3u; /* real gear/RPM value also survives */
    cpu.gpr[5] = 3u;
    assert(!r4_link_serial_filter(&cpu, 0x80021960u));
    assert(cpu.gpr[4] == 3u && cpu.gpr[5] == 1u);
    cpu.gpr[31] = 0x80000000u; /* only the authenticated overlay caller */
    cpu.gpr[5] = 3u;
    assert(!r4_link_serial_filter(&cpu, 0x80021960u));
    assert(cpu.gpr[5] == 3u);
    {
        const uint32_t root = 0x800ADCA0u + 0x2000u;
        const uint32_t extra = 0x800ADCA0u + 0x3000u;
        const uint32_t retail = 0x800ADCA0u + 0x4000u;
        psx_mod_write_word(0x800ACDCCu, 0x800ADCA0u);
        r4_link_hud_start[2] = extra;
        r4_link_hud_end[2] = extra + 20u;
        r4_link_hud_start[3] = r4_link_hud_end[3] = 0;
        psx_mod_write_word(root, extra & 0x00FFFFFFu);
        psx_mod_write_word(extra, 0x04000000u |
                                    (retail & 0x00FFFFFFu));
        psx_mod_write_word(extra + 4u, 0x65000000u);
        psx_mod_write_word(retail, 0x04FFFFFFu);
        psx_mod_write_word(retail + 4u, 0x65000000u);
        cpu.gpr[4] = root;
        r4_link_unlink_extra_hud(&cpu, 4);
        assert(psx_mod_read_word(root) == (retail & 0x00FFFFFFu));
        assert(psx_mod_read_word(extra) ==
               (0x04000000u | (retail & 0x00FFFFFFu)));
        assert(psx_mod_read_word(retail) == 0x04FFFFFFu);
    }
    /* Two seats: one car each, both views drawn, each peer presents its own
     * quadrant (not while the shared pause menu is up). */
    seat_count = 2;
    psx_mod_write_half(0x800FF838u, 3);
    psx_mod_write_word(0x800AC074u, 0x5A5A5A5Au);
    assert(!r4_link_serial_filter(&cpu, 0x80035EA0u));
    assert(psx_mod_read_word(0x800AC074u) == 0x00000101u);
    psx_mod_write_half(0x800AC754u, 4);
    psx_mod_write_byte(R4_LINK_PAUSED, 0);
    local_slot = 1;
    local_views = 0;
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(psx_mod_read_half(0x800AC754u) == 2);
    assert(local_views == 1);
    assert(local_view[0] == 160u && local_view[1] == 0u &&
           local_view[2] == 160u && local_view[3] == 120u);
    psx_mod_write_byte(R4_LINK_PAUSED, 1);
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(local_views == 1);
    psx_mod_write_byte(R4_LINK_PAUSED, 0);
    local_slot = 0;
    seat_count = 4;
    local_slot = 3;
    assert(!r4_link_serial_filter(&cpu, 0x80115770u));
    assert(local_views == 2);
    assert(local_view[0] == 160u && local_view[1] == 120u);
    local_slot = 0;
    seat_count = 2;
    {
        const uint32_t sp = 0x801FFE00u;
        cpu.gpr[29] = sp;
        psx_mod_write_word(0x800ACDCCu, 0x800ADCA0u);
        psx_mod_write_half(R4_LINK_ENTRANTS, 2);
        psx_mod_write_word(sp + 40u, 0);
        psx_mod_write_word(sp + 44u, 0x800ADCA0u + 0x70u);
        psx_mod_write_half(0x1F80005Eu, 0);
        assert(!r4_link_serial_filter(&cpu, 0x801157ECu));
        assert(psx_mod_read_word(sp + 40u) == 2u);
        assert(psx_mod_read_word(sp + 44u) == 0x800ADCA0u + 0xB70u);
        assert(psx_mod_read_half(0x1F80005Eu) == 1u);
        /* The two-view HUD setup and HUD pass see the two-view entrant
         * count; the real count returns right after each. */
        cpu.gpr[4] = 4u;
        cpu.gpr[31] = 0x8003D330u;
        assert(!r4_link_serial_filter(&cpu, 0x8002094Cu));
        assert(psx_mod_read_half(R4_LINK_ENTRANTS) == 4u);
        cpu.gpr[31] = 0x8003D3ACu;
        assert(!r4_link_serial_filter(&cpu, 0x8007807Cu));
        assert(psx_mod_read_half(R4_LINK_ENTRANTS) == 2u);
        cpu.gpr[31] = 0x80115F7Cu;
        assert(!r4_link_serial_filter(&cpu, 0x80021134u));
        assert(psx_mod_read_half(R4_LINK_ENTRANTS) == 4u);
        assert(!r4_link_serial_filter(&cpu, 0x80115F84u));
        assert(psx_mod_read_half(R4_LINK_ENTRANTS) == 2u);
        cpu.gpr[31] = 0x80000000u; /* other callers keep the real count */
        assert(!r4_link_serial_filter(&cpu, 0x80021134u));
        assert(psx_mod_read_half(R4_LINK_ENTRANTS) == 2u);
        /* Three or more entrants already take the two-view branch. */
        seat_count = 3;
        psx_mod_write_half(R4_LINK_ENTRANTS, 3);
        psx_mod_write_word(sp + 40u, 7u);
        assert(!r4_link_serial_filter(&cpu, 0x801157ECu));
        assert(psx_mod_read_word(sp + 40u) == 7u);
    }
    {
        /* A mode-4 frame without the race handler (results, Car Select)
         * keeps its OT: stale HUD copies are never linked in. */
        const uint32_t buf = 0x800ADCA0u;
        const uint32_t root = buf + 0x5000u;
        seat_count = 2;
        psx_mod_write_word(0x800ACDCCu, buf);
        psx_mod_write_word(root, 0x00FFFFFFu);
        r4_link_views_frame = 0;
        r4_link_hud_copy_head = r4_link_hud_copy_tail = buf + 0x6000u;
        cpu.gpr[4] = root;
        cpu.gpr[31] = 0x8001E828u;
        assert(!r4_link_serial_filter(&cpu, 0x80093520u));
        assert(psx_mod_read_word(root) == 0x00FFFFFFu);
        r4_link_hud_copy_head = r4_link_hud_copy_tail = 0;
    }
    seat_count = 4;
    sample_pad_valid[3] = 1;
    sample_pads[0].buttons = 0x7FFFu; /* Square: native brake ramp */
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x20900000u);
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x20940000u);
    for (int i = 0; i < 40; ++i)
        assert(r4_link_digital_command(0, &sample_pads[0]) <= 0x20FF0000u);
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x20FF0000u);
    memcpy(rollback_mod_ram, mod_ram, sizeof mod_ram);
    sample_pads[0].buttons = 0xFFFFu;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x20000000u);
    assert(mod_ram[0] == 0);
    sample_pads[0].analog = 1;
    sample_pads[0].lx = 128;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x80000000u);
    sample_pads[0].lx = 0;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x80000081u);
    sample_pads[0].lx = 64;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x800000CBu);
    sample_pads[0].lx = 192;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x80000035u);
    sample_pads[0].lx = 255;
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x8000007Eu);
    sample_pads[0].lx = 128;
    sample_pads[0].buttons = 0xFF7Fu; /* left with centered analog stick */
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x81000081u);
    sample_pads[0].buttons = 0xFFDFu; /* right with centered analog stick */
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x8200007Eu);
    sample_pads[0].lx = 160;
    sample_pads[0].buttons = 0xFF7Fu; /* stick steering takes precedence */
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x81000011u);
    sample_pads[0].analog = 0;
    sample_pads[0].buttons = 0xFFFFu;
    memcpy(mod_ram, rollback_mod_ram, sizeof mod_ram);
    assert(r4_link_digital_command(0, &sample_pads[0]) == 0x20000000u);
    assert(mod_ram[0] == 0);
    sample_pad_valid[2] = 0;
    psx_mod_write_byte(0x801190BCu, 5);
    psx_mod_write_byte(0x801190BDu, 5);
    assert(r4_link_serial_filter(0, 0x80115198u));
    assert(psx_mod_read_byte(0x801190BCu) == 5); /* no stale frame */
    sample_pad_valid[2] = 1;
    assert(r4_link_serial_filter(0, 0x80115198u));
    assert(psx_mod_read_byte(0x801190BCu) == 6); /* admitted frame */
    assert(r4_link_serial_filter(0, 0x80115198u));
    assert(psx_mod_read_byte(0x801190BCu) == 6); /* idempotent per consume */
    psx_mod_write_byte(0x801190BDu, 6); /* next consumed packet */
    psx_mod_write_byte(R4_LINK_PAUSE_LATCH, 5);
    memcpy(rollback_ram, ram, sizeof ram);
    memcpy(rollback_mod_ram, mod_ram, sizeof mod_ram);
    psx_mod_write_byte(R4_LINK_PAUSE_LATCH, 0);
    sample_pads[1].buttons = 0xFFFFu;
    assert(r4_link_serial_filter(0, 0x80115198u));
    assert(psx_mod_read_word(0x800ACDACu) == 0x20000000u);
    memcpy(ram, rollback_ram, sizeof ram); /* guest savestate rollback */
    memcpy(mod_ram, rollback_mod_ram, sizeof mod_ram);
    assert(psx_mod_read_byte(R4_LINK_PAUSE_LATCH) == 5);
    sample_pads[1].buttons = 0xBFFFu; /* sealed correction on replay */
    assert(r4_link_serial_filter(0, 0x80115198u));
    assert(psx_mod_read_byte(0x801190BCu) == 7);
    assert(psx_mod_read_word(0x800ACDACu) == 0x2000FF00u);
    assert(r4_link_serial_filter(0, 0x80115520u)); /* no SIO1 cable */
    psx_mod_write_word(0x801155CCu, 0);
    assert(!r4_link_serial_filter(0, 0x801155C4u));
    return 0;
}
