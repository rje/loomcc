#ifndef LOOM_CAMERA_H
#define LOOM_CAMERA_H

#include <loom/mode1.h>

/* How the camera keeps its target in view. */
#define LOOM_CAMERA_FOLLOW_CENTER ((loom_u8)0u)
#define LOOM_CAMERA_FOLLOW_DEAD_ZONE ((loom_u8)1u)
#define LOOM_CAMERA_FOLLOW_SCREENS ((loom_u8)2u)

/* Axis lock bits: a locked axis keeps the scene's initial camera value. */
#define LOOM_CAMERA_LOCK_X ((loom_u8)1u)
#define LOOM_CAMERA_LOCK_Y ((loom_u8)2u)

#define LOOM_CAMERA_MAX_DEAD_ZONE_WIDTH ((loom_u8)240u)
#define LOOM_CAMERA_MAX_DEAD_ZONE_HEIGHT ((loom_u8)208u)
#define LOOM_CAMERA_MAX_LOOK_AHEAD ((loom_u8)128u)
#define LOOM_CAMERA_MAX_AUTO_SCROLL ((loom_s16)4096)
#define LOOM_CAMERA_REGION_CAPACITY ((loom_u8)16u)

/* Follow behaviour. Auto-scroll is 1/256 pixel per tick; a non-zero axis
 * ignores the target on that axis. Smoothing moves an eighth of the
 * remaining distance per tick (at least one pixel) instead of snapping. */
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

/* A world-space box whose settings replace the scene's while the target's
 * anchor is inside it. The first matching region wins. */
typedef struct LoomCameraRegion {
    loom_s16 x;
    loom_s16 y;
    loom_u16 width;
    loom_u16 height;
    LoomCameraSettings settings;
} LoomCameraRegion;

/* How a scene with two players is framed. */
#define LOOM_CAMERA_TARGET_PLAYER_ONE ((loom_u8)0u)
/* The midpoint of the target and the second player, while one lives. */
#define LOOM_CAMERA_TARGET_MIDPOINT ((loom_u8)1u)

typedef struct LoomCameraScene {
    loom_s16 viewport_anchor_x;
    loom_s16 viewport_anchor_y;
    loom_u8 target_slot;
    loom_u8 region_count;
    LoomCameraSettings settings;
    const LoomCameraRegion *regions;
    /* Trailing so older literals zero-fill to player one. */
    loom_u8 target_mode;
    loom_u8 target_reserved;
} LoomCameraScene;

/* Defined by generated mode1_data.c for the selected initial scene. */
extern const loom_u8 loom_generated_camera_enabled;
extern const LoomCameraScene loom_generated_camera_initial_scene;

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* body.asm's camera tick: loom_camera_apply without the snap, reading the
 * camera state and the scene by offset (the layouts are asserted in
 * camera.c). Returns a LoomStatus. */
loom_u16 loom_pvs_camera_update(void);
#define LOOM_CAMERA_STATE_BYTES 32u
/* 816-tcc pads a struct holding a pointer to a multiple of four. */
#define LOOM_CAMERA_SCENE_BYTES 24u
#define LOOM_CAMERA_SETTINGS_BYTES 10u
#define LOOM_CAMERA_REGION_BYTES 18u
#endif
LoomStatus loom_camera_initialize(void);
LoomStatus loom_camera_activate_scene(const LoomCameraScene *scene);
/* The target's facing (-1, 0, 1 per axis) for look-ahead; the generated
 * schedule feeds the player's last movement direction before each update. */
void loom_camera_set_facing(loom_s8 x, loom_s8 y);
/* The second player's position this tick, or none; the generated schedule
 * feeds it from the actor pool before each update. */
void loom_camera_set_second_target(loom_s16 x, loom_s16 y, loom_u8 present);
LoomStatus loom_camera_update(void);
loom_s16 loom_camera_x(void);
loom_s16 loom_camera_y(void);

#endif
