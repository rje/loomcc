/* camera_follow: the camera's tick (Loom 925fc3a) over a 768x512 room, eight
 * ticks through seven scenes: a smoothed dead zone (two ticks, both
 * directions), screen paging, auto-scroll carrying its fraction, camera
 * regions (an X lock and a dead-zone Y), a two-player midpoint, centre follow
 * with look-ahead in a room no wider than the view, and a target slot with
 * no sprite. Between ticks the driver only moves the target sprite, switches
 * the scene, or (once) changes the room bounds -- in both Loom's camera state
 * and Mode 1's scene, and in the asm's Mode 1 binding, as activation does. */
#include "bench.h"
#include "loom_camera.h"

LoomCameraState loom_camera_state;
LoomMode1State loom_mode1_state;
loom_u8 loom_mode1_debug_epoch;

/* body.asm's Mode 1 binding (loom_pvs_mode1_bind in Loom); only the asm
 * reads these. */
loom_s16 *loom_pvs_mode1_world_x;
loom_s16 *loom_pvs_mode1_world_y;
loom_u8 *loom_pvs_mode1_slot_index;
loom_s16 *loom_pvs_mode1_camera;
loom_s16 loom_pvs_mode1_cmin_x;
loom_s16 loom_pvs_mode1_cmin_y;
loom_s16 loom_pvs_mode1_cmax_x;
loom_s16 loom_pvs_mode1_cmax_y;

static LoomMode1Scene mode1_scene;

/* bench_setup multiplies and divides by these so 816-tcc links tcc__mul and
 * tcc__div (the C unit's helpers) into the asm variant too: the harness
 * looks every helper label up in both. Setup is outside the measurement. */
static loom_s16 setup_two = 2;
static loom_s16 setup_minus_two = -2;

/* follow, axis_lock, dead_zone_width, dead_zone_height, look_ahead,
 * smoothing, auto_scroll_x, auto_scroll_y */
static const LoomCameraRegion regions[2] = {
    {0, 0, 128, 128, {0u, 0u, 0u, 0u, 0u, 0u, 0, 0}},
    {256, 128, 256, 200, {1u, LOOM_CAMERA_LOCK_X, 0u, 32u, 0u, 0u, 0, 0}},
};
/* anchor x, y, target slot, region count, settings, regions, target mode */
static const LoomCameraScene scene_center = {128, 112, 3u, 0u, {0u, 0u, 0u, 0u, 24u, 0u, 0, 0}, 0, 0u, 0u};
static const LoomCameraScene scene_dead = {128, 112, 3u, 0u, {1u, 0u, 64u, 48u, 0u, 1u, 0, 0}, 0, 0u, 0u};
static const LoomCameraScene scene_screens = {128, 112, 5u, 0u, {2u, 0u, 0u, 0u, 0u, 0u, 0, 0}, 0, 0u, 0u};
static const LoomCameraScene scene_auto = {128, 112, 3u, 0u, {0u, 0u, 0u, 0u, 0u, 0u, 0x0180, -0x00c0}, 0, 0u, 0u};
static const LoomCameraScene scene_regions = {128, 112, 3u, 2u, {0u, 0u, 0u, 0u, 0u, 0u, 0, 0}, regions, 0u, 0u};
static const LoomCameraScene scene_mid = {128, 112, 3u, 0u, {0u, 0u, 0u, 0u, 0u, 0u, 0, 0}, 0, LOOM_CAMERA_TARGET_MIDPOINT, 0u};
static const LoomCameraScene scene_bad = {128, 112, 9u, 0u, {0u, 0u, 0u, 0u, 0u, 0u, 0, 0}, 0, 0u, 0u};

#define TICKS 8
static loom_u16 status[TICKS];
static loom_s16 cam_x[TICKS];
static loom_s16 cam_y[TICKS];

/* The room's camera bounds, where activation leaves them. */
static void bounds(loom_s16 min_x, loom_s16 min_y, loom_s16 max_x, loom_s16 max_y)
{
    loom_camera_state.min_x = min_x;
    loom_camera_state.min_y = min_y;
    loom_camera_state.max_x = max_x;
    loom_camera_state.max_y = max_y;
    mode1_scene.camera_min_x = min_x;
    mode1_scene.camera_min_y = min_y;
    mode1_scene.camera_max_x = max_x;
    mode1_scene.camera_max_y = max_y;
    loom_pvs_mode1_cmin_x = min_x;
    loom_pvs_mode1_cmin_y = min_y;
    loom_pvs_mode1_cmax_x = max_x;
    loom_pvs_mode1_cmax_y = max_y;
}

void bench_setup(void)
{
    loom_u16 i;

    for (i = 0u; i <= LOOM_OAM_SLOT_MAX; i++)
        loom_mode1_state.slot_index[i] = 0xffu;
    loom_mode1_state.slot_index[0] = 0u;
    loom_mode1_state.slot_index[3] = 1u;
    loom_mode1_state.slot_index[5] = 2u;
    loom_mode1_state.sprite_world_x[2] = 600;
    loom_mode1_state.sprite_world_y[2] = 300;
    loom_mode1_state.initialized = LOOM_TRUE;
    loom_mode1_state.scene = &mode1_scene;
    loom_mode1_state.camera_x = 0;
    loom_mode1_state.camera_y = 0;

    loom_camera_state.initialized = LOOM_TRUE;
    loom_camera_state.facing_x = 1;
    loom_camera_state.facing_y = -1;
    loom_camera_state.second_x = (loom_s16)(setup_two * 250);
    loom_camera_state.second_y = (loom_s16)(-200 / setup_minus_two);
    loom_camera_state.second_present = LOOM_TRUE;

    loom_pvs_mode1_world_x = loom_mode1_state.sprite_world_x;
    loom_pvs_mode1_world_y = loom_mode1_state.sprite_world_y;
    loom_pvs_mode1_slot_index = loom_mode1_state.slot_index;
    loom_pvs_mode1_camera = &loom_mode1_state.camera_x;
    bounds(0, 0, 512, 288);
}

#define TARGET(x, y) (loom_mode1_state.sprite_world_x[1] = (x), loom_mode1_state.sprite_world_y[1] = (y))
#define TICK(i, s)                                   \
    do {                                             \
        loom_camera_state.scene = &(s);              \
        status[i] = loom_pvs_camera_update();        \
        cam_x[i] = loom_camera_state.camera_x;       \
        cam_y[i] = loom_camera_state.camera_y;       \
    } while (0)

void bench_run(void)
{
    TARGET(420, 260);
    TICK(0, scene_dead);
    TARGET(100, 150);
    TICK(1, scene_dead);
    TICK(2, scene_screens);
    TICK(3, scene_auto);
    TARGET(300, 200);
    TICK(4, scene_regions);
    TARGET(301, 201);
    TICK(5, scene_mid);
    bounds(64, 0, 64, 288);
    TICK(6, scene_center);
    TICK(7, scene_bad);
}

void bench_check(void)
{
    loom_u16 i, fold = 0u;

    for (i = 0u; i < TICKS; i++) {
        BENCH_OUT(cam_x[i]);
        BENCH_OUT(cam_y[i]);
        fold = (loom_u16)(fold * 8u + status[i]);
    }
    BENCH_OUT(fold);
    BENCH_OUT(status[TICKS - 1]);
    BENCH_OUT(loom_camera_state.auto_fraction_x);
    BENCH_OUT(loom_camera_state.auto_fraction_y);
    BENCH_OUT(loom_mode1_debug_epoch);
    BENCH_OUT(loom_mode1_state.camera_x);
    BENCH_OUT(loom_mode1_state.camera_y);
}
