#include "r4_camera_look.h"

/* These are behavioral tests in Release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>

#define PI 3.14159265f

static void target(uint8_t x, uint8_t y, float *yaw, float *pitch) {
    r4_camera_look_target(x, y, R4_CAMERA_LOOK_DEFAULT_DEADZONE, 1.0f,
                          yaw, pitch);
}

static float distance3(int32_t x, int32_t y, int32_t z) {
    return sqrtf((float)x * x + (float)y * y + (float)z * z);
}

/* P1 GTE records from the clean-road candidate run, RA 0x8002DAF8.
 * RT/TR, the mesh vertices, H=290, and OFX/OFY=(160,120) are copied from
 * frames 76631 (neutral) and 76679 (held side). Rear RT is derived by the
 * observed R4 world-Y half-turn applied to the recorded neutral RT. */
static const int32_t view_rt_neutral[9] = {
    4096, 0, 0, 0, 4095, 57, 0, -58, 4095
};
static const int32_t view_tr_neutral[3] = { 0, 250, 719 };
static const int32_t view_rt_side[9] = {
    38, 0, -4096, 44, 4096, 1, 4096, -44, 38
};
static const int32_t view_tr_side_failed[3] = { -722, 243, 4 };
static const int32_t player_car_vertices[][3] = {
    { -59, -42, -83 }, { -73, -35, -82 }, { -89, -42, -83 },
    { -74, -40, -83 }, { 73, -35, -82 }, { 59, -42, -83 },
    { 89, -42, -83 }, { 74, -40, -83 }, { -31, -46, -83 },
    { -31, -46, -83 }, { -31, -33, -81 }, { -90, -46, -83 },
    { -90, -33, -81 }, { 31, -33, -81 }, { 31, -46, -83 },
    { 90, -33, -81 }, { 90, -46, -83 },
};

typedef struct ProjectedPoint {
    double x;
    double y;
    double depth;
} ProjectedPoint;

static ProjectedPoint project_gte_vertex(const int32_t rt[9],
                                         const int32_t tr[3],
                                         const int32_t v[3]) {
    const double camera_x = tr[0] +
        (rt[0] * (double)v[0] + rt[1] * (double)v[1] +
         rt[2] * (double)v[2]) / 4096.0;
    const double camera_y = tr[1] +
        (rt[3] * (double)v[0] + rt[4] * (double)v[1] +
         rt[5] * (double)v[2]) / 4096.0;
    const double depth = tr[2] +
        (rt[6] * (double)v[0] + rt[7] * (double)v[1] +
         rt[8] * (double)v[2]) / 4096.0;
    ProjectedPoint point = { INFINITY, INFINITY, depth };
    if (depth > 0.0) {
        point.x = 160.0 + 290.0 * camera_x / depth;
        /* The recorded PSX GTE viewport has positive camera Y downwards. */
        point.y = 120.0 + 290.0 * camera_y / depth;
    }
    return point;
}

static int player_car_mesh_in_view(const int32_t rt[9],
                                   const int32_t tr[3]) {
    for (unsigned i = 0;
         i < sizeof(player_car_vertices) / sizeof(player_car_vertices[0]);
         ++i) {
        const ProjectedPoint p =
            project_gte_vertex(rt, tr, player_car_vertices[i]);
        if (p.depth <= 0.0 || p.x < 0.0 || p.x >= 320.0 ||
            p.y < 0.0 || p.y >= 240.0)
            return 0;
    }
    return 1;
}

static void rotate_half_turn_y(const int32_t rt[9], int32_t out[9]) {
    /* yaw 0x400 maps +X to -X and +Z to -Z in R4's traced GTE basis. */
    for (unsigned col = 0; col < 3; ++col) {
        out[col] = -rt[col];
        out[3 + col] = rt[3 + col];
        out[6 + col] = -rt[6 + col];
    }
}

static float radius_from_car(const R4CameraLookInput *camera,
                             int32_t car_x, int32_t car_y, int32_t car_z) {
    return distance3(camera->camera_x - car_x,
                     camera->camera_y - car_y,
                     camera->camera_z - car_z);
}

int main(void) {
    assert(R4_CAMERA_LOOK_INPUT_RESTORE_ENTRY == 0x800738C4u);
    assert(r4_camera_look_scene_eligible(1, 0, 1));
    assert(r4_camera_look_scene_eligible(3, 0, 1));
    assert(!r4_camera_look_scene_eligible(0, 0, 1));
    assert(!r4_camera_look_scene_eligible(4, 0, 1));
    assert(!r4_camera_look_scene_eligible(2, 1, 1));
    assert(r4_camera_look_scene_eligible(2, 0, 1)); /* race resumes */
    assert(!r4_camera_look_scene_eligible(2, 0, 0));

    float yaw, pitch;
    target(128, 128, &yaw, &pitch);
    assert(yaw == 0.0f && pitch == 0.0f);
    target(139, 130, &yaw, &pitch); /* small centre drift */
    assert(yaw == 0.0f && pitch == 0.0f);

    target(0, 128, &yaw, &pitch);
    assert(yaw < 0.0f && fabsf(yaw + PI * 0.5f) < 0.0001f);
    target(255, 128, &yaw, &pitch);
    assert(yaw > 0.0f && fabsf(yaw - PI * 0.5f) < 0.0001f);
    target(128, 255, &yaw, &pitch);
    assert(fabsf(yaw - PI) < 0.0001f && pitch == 0.0f);
    target(131, 255, &yaw, &pitch); /* down-stick rear target resists small X drift */
    assert(fabsf(yaw - PI) < 0.0001f && pitch == 0.0f);
    target(128, 0, &yaw, &pitch);
    assert(fabsf(yaw) < 0.0001f);
    assert(fabsf(pitch - R4_CAMERA_LOOK_MAX_PITCH_RAD) < 0.0001f);
    target(0, 255, &yaw, &pitch);
    assert(yaw < -PI * 0.5f && yaw > -PI);
    target(255, 255, &yaw, &pitch);
    assert(yaw > PI * 0.5f && yaw < PI);

    /* Exponential VBlank smoothing follows the same curve at both update
     * rates and returns to neutral after stick release. */
    R4CameraLookState at30 = {0.0f, 0.0f};
    R4CameraLookState at60 = {0.0f, 0.0f};
    for (int i = 0; i < 30; ++i)
        r4_camera_look_step(&at30, 1.2f, 0.1f, 1.0f / 30.0f);
    for (int i = 0; i < 60; ++i)
        r4_camera_look_step(&at60, 1.2f, 0.1f, 1.0f / 60.0f);
    assert(fabsf(at30.yaw_radians - at60.yaw_radians) < 0.0002f);
    assert(fabsf(at30.pitch_radians - at60.pitch_radians) < 0.0002f);
    const float held = at60.yaw_radians;
    r4_camera_look_step(&at60, 0.0f, 0.0f, 1.0f / 60.0f);
    assert(at60.yaw_radians < held && at60.yaw_radians > 0.0f);
    for (int i = 0; i < 240; ++i)
        r4_camera_look_step(&at60, 0.0f, 0.0f, 1.0f / 60.0f);
    assert(fabsf(at60.yaw_radians) < 0.001f);
    assert(fabsf(at60.pitch_radians) < 0.001f);

    /* Holding down reaches the rear view. Tiny X noise across the +PI/-PI seam
     * keeps the camera at the rear instead of sending it around the long way;
     * releasing the stick then returns smoothly to the centered view. */
    R4CameraLookState rear = {0.0f, 0.0f};
    for (int i = 0; i < 120; ++i)
        r4_camera_look_step(&rear, PI, 0.0f, 1.0f / 60.0f);
    assert(rear.yaw_radians > 3.0f);
    for (int i = 0; i < 8; ++i)
        r4_camera_look_step(&rear, -PI + 0.01f, 0.0f, 1.0f / 60.0f);
    assert(fabsf(rear.yaw_radians) > 3.0f);
    for (int i = 0; i < 240; ++i)
        r4_camera_look_step(&rear, 0.0f, 0.0f, 1.0f / 60.0f);
    assert(fabsf(rear.yaw_radians) < 0.001f);

    /* A close cockpit view keeps its seat anchor and turns only its view. */
    R4CameraLookState side = { PI * 0.5f, 0.0f };
    R4CameraLookInput cockpit = { 0, -100, 200, 0x100, 0x300 };
    const R4CameraLookInput cockpit_before = cockpit;
    const int cockpit_mode = r4_camera_look_adjust(&side, 0, 0, 0, &cockpit);
    assert(!cockpit_mode);
    assert(cockpit.camera_x == cockpit_before.camera_x);
    assert(cockpit.camera_y == cockpit_before.camera_y);
    assert(cockpit.camera_z == cockpit_before.camera_z);
    assert(cockpit.yaw_angle12 == 0x300u + 0x400u);
    R4CameraLookState cockpit_rear = { PI, 0.0f };
    R4CameraLookInput rear_seat = { 0, -100, 200, 0x100, 0x800 };
    assert(!r4_camera_look_adjust(&cockpit_rear, 0, 0, 0, &rear_seat));
    assert(rear_seat.camera_x == 0 && rear_seat.camera_y == -100 &&
           rear_seat.camera_z == 200);
    assert(rear_seat.yaw_angle12 == 0x000u);
    R4CameraLookState centered = { 0.0f, 0.0f };
    R4CameraLookInput cockpit_release = cockpit_before;
    assert(!r4_camera_look_adjust(&centered, 0, 0, 0, &cockpit_release));
    assert(cockpit_release.camera_x == cockpit_before.camera_x &&
           cockpit_release.camera_y == cockpit_before.camera_y &&
           cockpit_release.camera_z == cockpit_before.camera_z &&
           cockpit_release.yaw_angle12 == cockpit_before.yaw_angle12);

    /* Recorded clean-road chase sample (`camera-retest-d587fbb.json`, chase
     * 0): car (163643,-40,24632), eye (164369,-269,24632). Use its exact
     * (+726,-229,0) offset, rather than a synthetic large-radius orbit. The
     * actual P1 geometry/matrices below come from the separate GTE traces. */
    enum { car_x = 163643, car_y = -40, car_z = 24632 };
    R4CameraLookInput chase = { 164369, -269, 24632, 0, 0xC00 };
    const R4CameraLookInput chase_before = chase;
    const float chase_radius = radius_from_car(&chase, car_x, car_y, car_z);
    R4CameraLookState neutral = { 0.0f, 0.0f };
    assert(r4_camera_look_adjust(&neutral, car_x, car_y, car_z, &chase));
    assert(chase.camera_x == chase_before.camera_x &&
           chase.camera_y == chase_before.camera_y &&
           chase.camera_z == chase_before.camera_z &&
           chase.yaw_angle12 == chase_before.yaw_angle12);
    assert(player_car_mesh_in_view(view_rt_neutral, view_tr_neutral));

    chase = chase_before;
    const int chase_mode = r4_camera_look_adjust(&side, car_x, car_y, car_z,
                                                &chase);
    assert(chase_mode);
    assert(fabsf(radius_from_car(&chase, car_x, car_y, car_z) -
                 chase_radius) < 2.0f);
    assert(chase.camera_x == car_x);
    assert(chase.camera_y == chase_before.camera_y);
    assert(chase.camera_z == car_z - 726);
    assert(chase.yaw_angle12 == 0x000u);
    assert(chase.pitch_angle12 == chase_before.pitch_angle12);
    /* Live side trace: the angle turns, but TR remains the old-eye result
     * because the temporary position was restored at car-draw entry. The
     * player P1 calls still occur, but the submitted body has negative/near
     * depth and is off the viewport. Keeping the orbit eye through that call
     * makes camera-space TR match the neutral anchor and all captured vertices
     * stay in front of the PSX viewport. */
    assert(!player_car_mesh_in_view(view_rt_side, view_tr_side_failed));
    assert(player_car_mesh_in_view(view_rt_side, view_tr_neutral));

    /* The opposite orbit sign leaves the anchor behind the recorded side
     * view. This guards the eye transform against the earlier wrong-sign
     * correction even if a radius-only assertion would still pass. */
    const int32_t wrong_sign_tr[3] = { 0, 250, -719 };
    assert(!player_car_mesh_in_view(view_rt_side, wrong_sign_tr));

    /* Full rear yaw moves the eye to the car's front (-X) side. The GTE
     * forward row at yaw 0x400 points +X, back toward the anchor. */
    R4CameraLookState chase_rear = { PI, 0.0f };
    R4CameraLookInput rear_chase = chase_before;
    const int rear_chase_mode =
        r4_camera_look_adjust(&chase_rear, car_x, car_y, car_z,
                              &rear_chase);
    assert(rear_chase_mode);
    assert(fabsf(radius_from_car(&rear_chase, car_x, car_y, car_z) -
                 chase_radius) < 2.0f);
    assert(rear_chase.camera_x == car_x - 726);
    assert(rear_chase.camera_y == chase_before.camera_y);
    assert(rear_chase.camera_z == car_z);
    assert(rear_chase.yaw_angle12 == 0x400u);
    assert(rear_chase.pitch_angle12 == chase_before.pitch_angle12);
    int32_t view_rt_rear[9];
    rotate_half_turn_y(view_rt_neutral, view_rt_rear);
    assert(player_car_mesh_in_view(view_rt_rear, view_tr_neutral));

    R4CameraLookState chase_rear_approach = { PI * 0.75f, 0.0f };
    R4CameraLookInput rear_approach = chase_before;
    assert(r4_camera_look_adjust(&chase_rear_approach, car_x, car_y, car_z,
                                 &rear_approach));
    assert(rear_approach.camera_x < car_x &&
           rear_approach.camera_z < car_z);
    assert(rear_approach.yaw_angle12 == 0x200u);

    R4CameraLookInput chase_release = chase_before;
    assert(r4_camera_look_adjust(&centered, car_x, car_y, car_z,
                                 &chase_release));
    assert(chase_release.camera_x == chase_before.camera_x &&
           chase_release.camera_y == chase_before.camera_y &&
           chase_release.camera_z == chase_before.camera_z &&
           chase_release.yaw_angle12 == chase_before.yaw_angle12);

    R4CameraLookState up = { 0.0f, R4_CAMERA_LOOK_MAX_PITCH_RAD };
    R4CameraLookInput tilted = { 0, 0, 0, 0x100, 0x300 };
    (void)r4_camera_look_adjust(&up, 0, 0, 0, &tilted);
    assert(tilted.pitch_angle12 == 0x100u + 0x089u);
    assert(tilted.yaw_angle12 == 0x300u);

    /* Angles go back as signed deltas on the guest's own word: a slightly
     * negative pitch stays sign-extended instead of becoming 0xFFB. */
    assert(r4_camera_look_apply_angle(0xFFFFFFFBu, 0xFFBu) == 0xFFFFFFFBu);
    assert(r4_camera_look_apply_angle(0xFFFFFFFBu, 0x020u) == 0x00000020u);
    assert(r4_camera_look_apply_angle(0x00000987u, 0x187u) == 0x00000187u);
    assert(r4_camera_look_apply_angle(0x00000987u, 0xD87u) == 0x00000D87u);
    assert(r4_camera_look_apply_angle(0x00000FF0u, 0x010u) == 0x00000010u);
    assert(r4_camera_look_apply_angle(0xFFFFFFF0u, 0x010u) == 0x00000010u);
    return 0;
}
