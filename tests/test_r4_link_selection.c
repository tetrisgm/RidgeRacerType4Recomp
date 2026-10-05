/* Offline test of R4's generated link command selector and mode-4 dispatcher.
 *
 * Enter the 0x80029C3C basic block with synthetic guest RAM and run the real
 * recompiled code through its normal epilogue. This proves the two side
 * assignments and the current-vs-buffered local distinction. The dispatcher
 * test intercepts each call boundary and checks that four distinct car
 * pointers reach the command builder and car update in order. It does not
 * initialize a race, run car physics, or establish network determinism.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cpu_state.h"

extern void func_8002961C(CPUState *cpu);
extern void func_800382AC(CPUState *cpu);
uint32_t g_debug_last_store_pc;
volatile uint32_t g_psx_last_fn_entry;
static uint8_t ram[2u * 1024u * 1024u];
static int failures;

static uint32_t read_word(uint32_t addr) {
    uint32_t v;
    memcpy(&v, ram + (addr & 0x1ffffcu), 4);
    return v;
}
static uint16_t read_half(uint32_t addr) {
    uint16_t v;
    memcpy(&v, ram + (addr & 0x1ffffeu), 2);
    return v;
}
static void write_word(uint32_t addr, uint32_t v) {
    memcpy(ram + (addr & 0x1ffffcu), &v, 4);
}
static void write_half(uint32_t addr, uint16_t v) {
    memcpy(ram + (addr & 0x1ffffeu), &v, 2);
}
uint32_t psx_read_word(uint32_t a) { return read_word(a); }
uint16_t psx_read_half(uint32_t a) { return read_half(a); }
uint8_t psx_cyc_load_byte(CPUState *cpu, uint32_t a, uint32_t rt, uint32_t mask) {
    (void)cpu; (void)rt; (void)mask;
    return ram[a & 0x1fffffu];
}
void psx_check_interrupts_at(CPUState *cpu, uint32_t pc) { (void)cpu; (void)pc; }

int main(void) {
    const uint32_t local[2] = {0x12121212u, 0x34343434u};
    const uint32_t remote[2] = {0x56565656u, 0x78787878u};
    for (unsigned side = 0; side < 2; ++side) {
        for (unsigned slot = 0; slot < 4; ++slot) {
            CPUState cpu = {0};
            memset(ram, 0, sizeof ram);
            for (unsigned i = 0; i < 2; ++i) {
                write_word(0x800ACDA8u + 4*i, local[i]);
                write_word(0x800ACD98u + 4*i, remote[i]);
            }
            write_word(0x800BDC88u + 4*slot, 0xA0A0A000u + slot);
            write_half(0x800AD6C0u, (uint16_t)side);
            write_word(0x800AD6BCu, 0u);
            cpu.pc = 0x80029C3Cu;
            cpu.gpr[14] = slot;                  /* t6, racer index */
            cpu.gpr[10] = slot & 1u;             /* t2, command-pair index */
            cpu.gpr[6] = 0x800BDC88u + 4*slot; /* a2, destination */
            cpu.gpr[29] = 0x801FF000u;
            cpu.gpr[31] = 0x80000000u;
            cpu.write_word = write_word;
            cpu.read_word = read_word;
            cpu.read_half = read_half;
            func_8002961C(&cpu);
            uint32_t actual = read_word(0x800BDC88u + 4*slot);
            int is_local = side ? slot < 2 : slot >= 2;
            uint32_t expected = is_local ? local[slot & 1u] : remote[slot & 1u];
            if (actual != expected) {
                fprintf(stderr, "side=%u slot=%u actual=%08x expected=%08x\n", side, slot, actual, expected);
                ++failures;
            }
            uint32_t staged = read_word(0x800F3CA0u + 4*(slot & 1u));
            uint32_t expected_stage = is_local ? 0xA0A0A000u + slot : 0u;
            if (staged != expected_stage) {
                fprintf(stderr, "side=%u slot=%u staged=%08x expected=%08x\n", side, slot, staged, expected_stage);
                ++failures;
            }
        }
    }
    if (failures) return 1;
    puts("PASS: eight native link selection cases from the generated R4 function");

    {
        CPUState cpu = {0};
        const uint32_t cars[4] = {0x800AC0B0u, 0x800AC3D0u, 0x80107348u, 0x80107668u};
        memset(ram, 0, sizeof ram);
        write_half(0x800AC754u, 4);
        for (unsigned i = 0; i < 4; ++i) {
            write_word(0x800FFDD0u + 4*i, cars[i]);
            write_half(cars[i] + 0x1E8u, 0);
        }
        cpu.pc = 0x800383E8u;
        cpu.read_word = read_word;
        cpu.read_half = read_half;
        cpu.write_word = write_word;
        for (unsigned phase = 0; phase < 2; ++phase) {
            for (unsigned i = 0; i < 4; ++i) {
                func_800382AC(&cpu);
                uint32_t target = phase ? 0x80022B20u : 0x8002961Cu;
                uint32_t next = phase ? 0x80038470u : 0x80038418u;
                if (cpu.pc != target || cpu.gpr[4] != cars[i] || cpu.gpr[5] != i) {
                    fprintf(stderr,"phase=%u i=%u pc=%08x a0=%08x a1=%08x\n",
                            phase,i,cpu.pc,cpu.gpr[4],cpu.gpr[5]);
                    ++failures;
                }
                cpu.pc = next; /* return from the intercepted callee */
            }
        }
        if (failures) return 1;
        puts("PASS: native mode-4 dispatcher calls command builder and car update for all four cars");
    }
    return 0;
}
