/* Effects consume forward guest scene changes, never presentation time. */
#pragma once
#include "r4_race_state.h"
#include <string.h>
struct R4EffectsClock {
    R4RaceState previous{};
    bool initialized = false;
    bool step(const R4RaceState &scene, bool rewind) {
        R4RaceState a = scene, b = previous;
        a.frame = b.frame = a.vblank = b.vblank = 0;
        a.paused = b.paused = 0;
        /* These ages belong to the sampler, which also runs during pause. */
        for (int i = 0; i < R4_RACE_CARS; ++i) {
            a.cars[i].since_shift = b.cars[i].since_shift = 0;
            a.cars[i].since_impact = b.cars[i].since_impact = 0;
        }
        const uint32_t elapsed = scene.vblank - previous.vblank;
        const bool advance = initialized && !rewind && !scene.paused &&
            !previous.paused && scene.in_race && previous.in_race &&
            scene.frame != previous.frame && elapsed > 0 && elapsed <= 8 &&
            memcmp(&a, &b, sizeof a) != 0;
        previous = scene;
        initialized = true;
        return advance;
    }
};
