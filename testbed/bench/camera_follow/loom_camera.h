/* camera_follow: the Loom types the camera tick reads, copied from
 * runtime/include/loom/{types,camera,mode1,video}.h and runtime/src/camera.c
 * at Loom 925fc3a^. LoomCameraSettings, LoomCameraRegion, LoomCameraScene and
 * LoomCameraState are verbatim (body.asm reads them by offset). Mode 1's
 * state and scene are cut down to the fields the camera path reads, in their
 * original order: the asm never sees their layout (it reads Mode 1 through
 * pointers bound at activation). */
#ifndef CAMERA_FOLLOW_LOOM_CAMERA_H
#define CAMERA_FOLLOW_LOOM_CAMERA_H

typedef signed char loom_s8;
typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;
typedef loom_u8 LoomStatus;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_STATUS_OK ((LoomStatus)0u)
#define LOOM_STATUS_NOT_READY ((LoomStatus)1u)
#define LOOM_STATUS_INVALID_ARGUMENT ((LoomStatus)3u)
#define LOOM_STATUS_INVALID_HANDLE ((LoomStatus)4u)
#define LOOM_STATIC_ASSERT(name, condition) \
    typedef char loom_static_assert_##name[(condition) ? 1 : -1]

#define LOOM_MODE1_VIEW_WIDTH ((loom_u16)256u)
#define LOOM_MODE1_VIEW_HEIGHT ((loom_u16)224u)
#define LOOM_OAM_SLOT_MAX ((loom_u8)127u)
#define LOOM_FRAME_OAM_CAPACITY ((loom_u8)33u)

#define LOOM_CAMERA_FOLLOW_CENTER ((loom_u8)0u)
#define LOOM_CAMERA_FOLLOW_DEAD_ZONE ((loom_u8)1u)
#define LOOM_CAMERA_FOLLOW_SCREENS ((loom_u8)2u)
#define LOOM_CAMERA_LOCK_X ((loom_u8)1u)
#define LOOM_CAMERA_LOCK_Y ((loom_u8)2u)
#define LOOM_CAMERA_TARGET_PLAYER_ONE ((loom_u8)0u)
#define LOOM_CAMERA_TARGET_MIDPOINT ((loom_u8)1u)

typedef struct LoomCameraSettings {
    loom_u8 follow;
    loom_u8 axis_lock;
    loom_u8 dead_zone_width;
    loom_u8 dead_zone_height;
    loom_u8 look_ahead;
    loom_u8 smoothing;
    loom_s16 auto_scroll_x;
    loom_s16 auto_scroll_y;
} LoomCameraSettings;

typedef struct LoomCameraRegion {
    loom_s16 x;
    loom_s16 y;
    loom_u16 width;
    loom_u16 height;
    LoomCameraSettings settings;
} LoomCameraRegion;

typedef struct LoomCameraScene {
    loom_s16 viewport_anchor_x;
    loom_s16 viewport_anchor_y;
    loom_u8 target_slot;
    loom_u8 region_count;
    LoomCameraSettings settings;
    const LoomCameraRegion *regions;
    loom_u8 target_mode;
    loom_u8 target_reserved;
} LoomCameraScene;

/* runtime/src/camera.c (static there at 925fc3a^; exported at 925fc3a so
 * body.asm can read it). */
typedef struct LoomCameraState {
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 initialized;
    loom_s16 auto_fraction_x;
    loom_s16 auto_fraction_y;
    loom_s8 facing_x;
    loom_s8 facing_y;
    loom_u8 reserved;
    loom_s16 second_x;
    loom_s16 second_y;
    loom_u8 second_present;
    const LoomCameraScene *scene;
    loom_s16 min_x;
    loom_s16 min_y;
    loom_s16 max_x;
    loom_s16 max_y;
} LoomCameraState;

#if defined(__65816__)
/* The sizes camera.c asserts at 925fc3a. */
LOOM_STATIC_ASSERT(camera_state_bytes, sizeof(LoomCameraState) == 32u);
LOOM_STATIC_ASSERT(camera_scene_bytes, sizeof(LoomCameraScene) == 24u);
LOOM_STATIC_ASSERT(camera_settings_bytes, sizeof(LoomCameraSettings) == 10u);
LOOM_STATIC_ASSERT(camera_region_bytes, sizeof(LoomCameraRegion) == 18u);
#endif

extern LoomCameraState loom_camera_state;

/* Mode 1, cut down (see above). */
typedef struct LoomMode1Scene {
    loom_s16 camera_min_x;
    loom_s16 camera_min_y;
    loom_s16 camera_max_x;
    loom_s16 camera_max_y;
} LoomMode1Scene;

typedef struct LoomMode1State {
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 initialized;
    const LoomMode1Scene *scene;
    loom_s16 sprite_world_x[LOOM_FRAME_OAM_CAPACITY];
    loom_s16 sprite_world_y[LOOM_FRAME_OAM_CAPACITY];
    loom_u8 slot_index[LOOM_OAM_SLOT_MAX + 1u];
} LoomMode1State;

extern LoomMode1State loom_mode1_state;
extern loom_u8 loom_mode1_debug_epoch;

loom_u16 loom_pvs_camera_update(void);

#endif
