/* Race state for R4's host-side effects (race HUD, lighting chain, car
 * effects). Sampled from guest RAM on every view build (0x8006E4D0) and read
 * from the render thread through r4_race_state_read (a consistent copy).
 *
 * Layout (static analysis, 2026-10-19):
 *  - car: world position +0x10/+0x14/+0x18 (s32, y down), heading +0x54,
 *    pitch +0x50, roll +0x58 (s32, 4096 = one turn), direction of motion
 *    +0x1D4 (s16), speed +0x1D8 (s16), velocity +0x30/+0x38 (s32), rpm +0x27C,
 *    gear +0x27A, rank +0x1EE, lap +0x2AA; slip = signed motion - heading
 *    difference (0x80025F78).
 *  - camera (scratchpad inputs of 0x8006E4D0): eye 0x1F800008 (3 x s32),
 *    pitch 0x1F800018, yaw 0x1F80001C, roll 0x1F800020;
 *    view = Rz(roll) * Rx(pitch) * Ry(yaw);
 *    camera space = view * ((world - eye) >> 2) / 4096 (0x8002E554).
 *  - per-view projection (0x8006F2B0): current view 0x800AC790,
 *    OFX s16 0x800A3D5C[v], OFY s16 0x800A3D6C[v], H s32 0x800A3D7C[v].
 *  - light: world light matrix 0x800AC088 (3 x 3 s16, 4096 = 1), blended per
 *    track zone (0x800138FC, 0x2C-byte records at *0x800F4F40); row 0 is the
 *    dominant light. */
#ifndef R4_RACE_STATE_H
#define R4_RACE_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define R4_RACE_CARS 8
#define R4_RACE_VIEWS 3
/* World units per visual meter (car seen from above in the lab, about 4.5 m
 * long), 2026-10-20. A car's origin is on its axis, about 0.9 m ahead of its
 * rear face and 0.55 m above the ground. */
#define R4_METER 125.f

typedef struct R4RaceCar {
    int32_t x, y, z;            /* world */
    int32_t heading, pitch, roll;   /* 0..4095 */
    int32_t motion_heading;     /* direction of motion, 0..4095 */
    int32_t slip;               /* motion - heading, -2048..2047 (drift) */
    int32_t speed;              /* raw (+0x1D8) */
    int32_t vx, vz;             /* +0x30 / +0x38 */
    int32_t rpm, gear, rank, lap;
    /* events, in game frames (30/s) since the event; -1: none */
    int32_t since_shift;        /* last gear change */
    int32_t shift_dir;          /* +1 up, -1 down */
    int32_t since_impact;       /* last sudden speed loss (wall, car) */
    int32_t impact_loss;        /* speed lost in that impact */
} R4RaceCar;

typedef struct R4RaceCamera {
    int valid;
    int32_t eye[3];
    int32_t pitch, yaw, roll;
    float matrix[3][3];         /* rows, 1.0 = 4096 */
    int32_t ofx, ofy, h;        /* view projection (PS1 pixels); h = GTE focal length */
    int32_t h_table;            /* focal length from the view table (0x800A3D7C) */
} R4RaceCamera;

typedef struct R4RaceState {
    uint32_t frame;             /* game frames (0x800AC064) */
    uint32_t vblank;            /* game VBlanks (VSync(-1), 0x800A6548, 60/s) at this
                                 * frame: a game frame lasts two, more when the game
                                 * slows down */
    int in_race;
    int paused;                /* guest pause, sampled with the scene */
    int speedometer;
    int car_count;
    R4RaceCar cars[R4_RACE_CARS];          /* [0] = player 1 */
    R4RaceCamera cameras[R4_RACE_VIEWS];   /* per view (0: full screen; 1, 2: split screen) */
    float light_dir[3];         /* dominant light direction (world, normalized) */
} R4RaceState;

/* Consistent copy of the latest state (render or game thread). 0 if there is
 * no state yet. */
int r4_race_state_read(R4RaceState *out);
/* Same, the full state of the game frame `delay` frames ago (1: the one on
 * screen while the game draws the next), else the latest. */
int r4_race_state_read_delayed(R4RaceState *out, int delay);

/* World point -> PS1 pixels of the view (x: 0..320, y: 0..240) and camera
 * depth (units >> 2). 0 if behind the camera or the camera is unknown. */
int r4_race_project(const R4RaceState *s, int view, float wx, float wy, float wz,
                    float *sx, float *sy, float *depth);

#ifdef __cplusplus
}
#endif

#endif
