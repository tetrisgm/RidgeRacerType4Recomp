#ifndef R4_CAMERA_LOOK_H
#define R4_CAMERA_LOOK_H

#include <math.h>
#include <stdint.h>

typedef struct R4CameraLookState {
    float yaw_radians;
    float pitch_radians;
} R4CameraLookState;

typedef struct R4CameraLookInput {
    int32_t camera_x;
    int32_t camera_y;
    int32_t camera_z;
    uint32_t pitch_angle12;
    uint32_t yaw_angle12;
} R4CameraLookInput;

enum {
    R4_CAMERA_LOOK_WORLD_ONE = 4096,
    /* R4's chase view starts about 0.18 units from the car origin; the
     * cockpit anchor stays within 0.06 units at race speed. */
    R4_CAMERA_LOOK_COCKPIT_RADIUS = R4_CAMERA_LOOK_WORLD_ONE / 10,
    R4_CAMERA_LOOK_ANGLE_UNITS = 4096,
};

/* The camera-state updater rewrites the scratch eye before each view build.
 * Restore the prior frame's temporary eye at this entry, before that update
 * runs; restoring at the car-draw entry is too early for its GTE transform. */
#define R4_CAMERA_LOOK_INPUT_RESTORE_ENTRY 0x800738C4u

#define R4_CAMERA_LOOK_DEFAULT_DEADZONE 0.18f
#define R4_CAMERA_LOOK_PI 3.14159265f
#define R4_CAMERA_LOOK_TWO_PI 6.28318531f
#define R4_CAMERA_LOOK_MAX_PITCH_RAD 0.20943951f /* 12 degrees */

static inline int r4_camera_look_scene_eligible(uint32_t phase,
                                                 uint8_t paused,
                                                 int race_handler) {
    return phase >= 1u && phase <= 3u && paused == 0u && race_handler != 0;
}

static inline float r4_camera_look_axis(int value) {
    const int delta = value - 128;
    return delta < 0 ? (float)delta / 128.0f : (float)delta / 127.0f;
}

/* Map the right stick as a horizontal look vector: full left/right gives a
 * 90-degree side view, full down gives a 180-degree rear look, and diagonals
 * point between those views. Up only adds a small upward pitch. */
static inline void r4_camera_look_target(uint8_t right_x, uint8_t right_y,
                                         float deadzone, float sensitivity,
                                         float *yaw_out, float *pitch_out) {
    float x = r4_camera_look_axis((int)right_x);
    float y = r4_camera_look_axis((int)right_y);
    float radius = sqrtf(x * x + y * y);
    if (radius > 1.0f) {
        x /= radius;
        y /= radius;
        radius = 1.0f;
    }
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 0.90f) deadzone = 0.90f;
    if (sensitivity < 0.0f) sensitivity = 0.0f;
    if (sensitivity > 2.0f) sensitivity = 2.0f;
    if (radius <= deadzone || radius == 0.0f) {
        *yaw_out = 0.0f;
        *pitch_out = 0.0f;
        return;
    }

    const float reach = (radius - deadzone) / (1.0f - deadzone);
    const float scale = reach / radius;
    x *= scale;
    y *= scale;
    /* Snap tiny horizontal drift while the stick is held down to a stable
     * straight-behind target instead of letting noise flip +PI/-PI. */
    if (y > 0.0f && fabsf(x) < 0.05f * y) x = 0.0f;
    *yaw_out = atan2f(x, -y) * reach * sensitivity;
    if (*yaw_out > R4_CAMERA_LOOK_PI) *yaw_out = R4_CAMERA_LOOK_PI;
    if (*yaw_out < -R4_CAMERA_LOOK_PI) *yaw_out = -R4_CAMERA_LOOK_PI;

    const float look_up = -y > 0.0f ? -y : 0.0f;
    *pitch_out = look_up * R4_CAMERA_LOOK_MAX_PITCH_RAD * sensitivity;
    if (*pitch_out > R4_CAMERA_LOOK_MAX_PITCH_RAD * 2.0f)
        *pitch_out = R4_CAMERA_LOOK_MAX_PITCH_RAD * 2.0f;
}

/* Choose the shortest azimuth step. This keeps stick jitter across the
 * +PI/-PI seam near a single rear-view target instead of spinning through the
 * front of the car. Preserve the command's sign for the exact 180-degree tie. */
static inline float r4_camera_look_yaw_delta(float current, float target) {
    const float raw = target - current;
    float delta = atan2f(sinf(raw), cosf(raw));
    if (fabsf(fabsf(delta) - R4_CAMERA_LOOK_PI) < 0.00001f)
        delta = raw < 0.0f ? -R4_CAMERA_LOOK_PI : R4_CAMERA_LOOK_PI;
    return delta;
}

/* Keep the local azimuth in one revolution while preserving its orientation. */
static inline float r4_camera_look_wrap_yaw(float radians) {
    float wrapped = fmodf(radians + R4_CAMERA_LOOK_PI,
                          R4_CAMERA_LOOK_TWO_PI);
    if (wrapped < 0.0f) wrapped += R4_CAMERA_LOOK_TWO_PI;
    return wrapped - R4_CAMERA_LOOK_PI;
}

/* One NTSC VBlank step, independent of how many render presents occur. */
static inline void r4_camera_look_step(R4CameraLookState *state,
                                       float target_yaw,
                                       float target_pitch,
                                       float dt_seconds) {
    if (dt_seconds < 0.0f) dt_seconds = 0.0f;
    if (dt_seconds > 0.25f) dt_seconds = 0.25f;
    const float alpha = 1.0f - expf(-dt_seconds / 0.12f);
    state->yaw_radians +=
        r4_camera_look_yaw_delta(state->yaw_radians, target_yaw) * alpha;
    state->yaw_radians = r4_camera_look_wrap_yaw(state->yaw_radians);
    state->pitch_radians += (target_pitch - state->pitch_radians) * alpha;
    if (fabsf(state->yaw_radians) < 0.00001f &&
        fabsf(target_yaw) < 0.00001f)
        state->yaw_radians = 0.0f;
    if (fabsf(state->pitch_radians) < 0.00001f &&
        fabsf(target_pitch) < 0.00001f)
        state->pitch_radians = 0.0f;
}

static inline int32_t r4_camera_look_angle_delta(float radians) {
    const float units = radians * ((float)R4_CAMERA_LOOK_ANGLE_UNITS / 6.28318531f);
    return (int32_t)lrintf(units);
}

/* Re-apply an adjusted 12-bit angle to the guest's original word, keeping
 * the word's own representation: a word already in 0..4095 stays masked, while
 * R4's sign-extended small negative pitches take the signed delta in
 * [-2048, 2047]. An unchanged angle returns the original word bit-for-bit. */
static inline uint32_t r4_camera_look_apply_angle(uint32_t original_word,
                                                  uint32_t adjusted12) {
    if (original_word <= 0xFFFu) return adjusted12 & 0xFFFu;
    int32_t delta = ((int32_t)(adjusted12 & 0xFFFu) -
                     (int32_t)(original_word & 0xFFFu)) & 0xFFF;
    if (delta >= 2048) delta -= 4096;
    return original_word + (uint32_t)delta;
}

/* Adjust the camera-builder inputs. Cockpit cameras keep their anchor in the
 * driver's seat. For chase cameras, R4's RotY view basis has forward
 * (sin(yaw), cos(yaw)) in the world XZ plane, so the eye offset must rotate by
 * the inverse yaw to keep the car ahead of the camera. R4 world coordinates
 * use 4096 integer units per game unit, as used by the camera and car blocks. */
static inline int r4_camera_look_adjust(const R4CameraLookState *look,
                                        int32_t car_x, int32_t car_y,
                                        int32_t car_z,
                                        R4CameraLookInput *camera) {
    const double dx = (double)camera->camera_x - (double)car_x;
    const double dy = (double)camera->camera_y - (double)car_y;
    const double dz = (double)camera->camera_z - (double)car_z;
    const double cockpit_radius = (double)R4_CAMERA_LOOK_COCKPIT_RADIUS;
    const int chase = dx * dx + dy * dy + dz * dz >
                      cockpit_radius * cockpit_radius;
    if (chase && fabsf(look->yaw_radians) > 0.00001f) {
        /* The view turns by +yaw, but the eye has to move around the opposite
         * side of the car. At yaw +90 the view looks +Z, so the eye belongs at
         * -Z; at +180 it belongs on the car's front (-X) side. */
        const double c = cos((double)look->yaw_radians);
        const double s = sin((double)look->yaw_radians);
        const double x = dx * c + dz * s;
        const double z = -dx * s + dz * c;
        camera->camera_x = (int32_t)lrint((double)car_x + x);
        camera->camera_z = (int32_t)lrint((double)car_z + z);
    }
    if (fabsf(look->yaw_radians) > 0.00001f)
        camera->yaw_angle12 =
            (camera->yaw_angle12 +
             (uint32_t)r4_camera_look_angle_delta(look->yaw_radians)) & 0xFFFu;
    if (fabsf(look->pitch_radians) > 0.00001f)
        camera->pitch_angle12 =
            (camera->pitch_angle12 +
             (uint32_t)r4_camera_look_angle_delta(look->pitch_radians)) & 0xFFFu;
    return chase;
}

#endif
