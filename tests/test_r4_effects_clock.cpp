#include "r4_effects_clock.h"
#include <cstdlib>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main() {
    R4EffectsClock clock;
    R4RaceState s{}; s.in_race = 1; s.frame = 100; s.vblank = 200;
    CHECK(!clock.step(s, false));
    s.frame++; s.vblank += 2; s.cars[0].x++;
    CHECK(clock.step(s, false));
    for (int i=0; i<120; ++i) CHECK(!clock.step(s, false));
    s.paused = 1;
    for (int i=0; i<120; ++i) {
        s.frame++; s.vblank += 2; s.cars[0].since_shift++;
        CHECK(!clock.step(s, false));
    }
    s.paused = 0; CHECK(!clock.step(s, false));
    for (int i=0; i<120; ++i) {
        s.frame++; s.vblank += 2; CHECK(!clock.step(s, false));
    }
    s.frame++; s.vblank += 2; s.cars[0].x++;
    CHECK(clock.step(s, false));
    s.frame++; s.vblank += 2; s.cars[0].x++;
    CHECK(!clock.step(s, true));
    s.frame -= 50; s.vblank -= 100; s.cars[0].x -= 50;
    CHECK(!clock.step(s, false));
    s.frame++; s.vblank += 2; s.cars[0].x++;
    CHECK(clock.step(s, false));
    s.frame += 100; s.vblank += 200; s.cars[0].x += 100;
    CHECK(!clock.step(s, false));
    return 0;
}
