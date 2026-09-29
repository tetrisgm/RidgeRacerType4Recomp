/* r4_widescreen_scene.h - "is this frame a race?" for R4 (US).
 *
 * Pure helpers (no runtime dependencies) so tests/test_r4_widescreen.c can
 * check them against fake RAM. Races widen; menus, results, the garage and
 * movies stay 4:3.
 *
 * The main loop (0x8001E784-0x8001E7B0) calls the frame handler
 *     T[*(u16 *)0x800F4E1A][*(u16 *)0x800F3BD6],   T = 0x8009EBCC
 * and a race runs one of five handlers: two in the EXE and three in code
 * overlays linked at 0x801149A8. An overlay address can hold other code when
 * a different overlay is resident, so each overlay handler is also matched on
 * its first two instruction words. The race phase *(u32 *)0x800FF860 is 0
 * (fly-by), 1 (countdown) or 2..3 (racing); 4 and up are the finish, results
 * and replays, which stay 4:3.
 *
 * The phase gate and the five handlers follow the race predicate in
 * dogewow2048's R4 JP patch analysis (credit: dogewow2048); addresses and
 * guard words are the US ones.
 */
#ifndef R4_WIDESCREEN_SCENE_H
#define R4_WIDESCREEN_SCENE_H

#include <stdint.h>

#define R4_RACE_PHASE_ADDR     0x800FF860u
#define R4_STATE_MAJOR_ADDR    0x800F4E1Au   /* u16, rows 0..4 */
#define R4_STATE_MINOR_ADDR    0x800F3BD6u   /* u16, slots 0..89 */
#define R4_HANDLER_TABLE_ADDR  0x8009EBCCu
#define R4_STATE_MAJOR_MAX     4u
#define R4_STATE_MINOR_MAX     89u

typedef struct {
    uint32_t handler;
    uint32_t word0, word1;   /* first two instructions; 0,0 = EXE code */
} R4RaceHandler;

#define R4_RACE_HANDLER_COUNT 5
static const R4RaceHandler r4_race_handlers[R4_RACE_HANDLER_COUNT] = {
    { 0x8005E118u, 0u, 0u },                       /* attract demo (EXE) */
    { 0x8002A464u, 0u, 0u },                       /* after-goal run (EXE) */
    { 0x8011729Cu, 0x3C04800Fu, 0x3C038010u },     /* race, overlay 660 */
    { 0x80114A38u, 0x27BDFFC8u, 0x3C03800Fu },     /* race, overlay 659 */
    { 0x80114C30u, 0x3C04800Fu, 0x3C038010u },     /* 2P VS race, overlay 661 */
};

typedef uint32_t (*R4SceneReadWord)(uint32_t address);
typedef uint16_t (*R4SceneReadHalf)(uint32_t address);

/* The current frame handler, or 0 when the state indices are out of range. */
static inline uint32_t r4_frame_handler(R4SceneReadWord rd, R4SceneReadHalf rh)
{
    uint32_t major = rh(R4_STATE_MAJOR_ADDR);
    uint32_t minor = rh(R4_STATE_MINOR_ADDR);
    if (major > R4_STATE_MAJOR_MAX || minor > R4_STATE_MINOR_MAX) return 0;
    uint32_t row = rd(R4_HANDLER_TABLE_ADDR + 4u * major);
    if ((row & 0xFFE00003u) != 0x80000000u) return 0;   /* not a RAM pointer */
    return rd(row + 4u * minor);
}

/* 1 when `handler` is one of the race handlers (with its code resident). */
static inline int r4_is_race_handler(R4SceneReadWord rd, uint32_t handler)
{
    for (int i = 0; i < R4_RACE_HANDLER_COUNT; i++) {
        const R4RaceHandler *h = &r4_race_handlers[i];
        if (h->handler != handler) continue;
        if (!h->word0) return 1;
        return rd(handler) == h->word0 && rd(handler + 4u) == h->word1;
    }
    return 0;
}

/* Pure reads only: this runs inside GPU scene classification. */
static inline int r4_in_race(R4SceneReadWord rd, R4SceneReadHalf rh)
{
    if (rd(R4_RACE_PHASE_ADDR) >= 4u) return 0;
    return r4_is_race_handler(rd, r4_frame_handler(rd, rh));
}

#endif /* R4_WIDESCREEN_SCENE_H */
