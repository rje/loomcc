#ifndef LOOM_SCENE_H
#define LOOM_SCENE_H

#include <loom/actor.h>
#include <loom/animation.h>
#include <loom/adventure.h>
#include <loom/camera.h>
#include <loom/movement.h>

#define LOOM_SCENE_TRIGGER_CAPACITY ((loom_u16)16u)
#define LOOM_SCENE_INVALID_INDEX ((loom_u16)0xffffu)

#define LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY ((loom_u8)0x01u)
#define LOOM_SCENE_TRIGGER_EXIT ((loom_u8)0x02u)

#define LOOM_SCENE_TRANSITION_READY ((loom_u8)0u)
#define LOOM_SCENE_TRANSITION_FADE_OUT ((loom_u8)1u)
#define LOOM_SCENE_TRANSITION_BLACK_WAIT ((loom_u8)2u)
#define LOOM_SCENE_TRANSITION_LOAD ((loom_u8)3u)
#define LOOM_SCENE_TRANSITION_FADE_IN ((loom_u8)4u)
#define LOOM_SCENE_TRANSITION_FINAL_WAIT ((loom_u8)5u)

typedef struct LoomSceneTrigger {
    loom_s16 x;
    loom_s16 y;
    loom_u16 width;
    loom_u16 height;
    loom_u16 first_hook;
    loom_u16 target_scene;
    loom_s16 target_spawn_x;
    loom_s16 target_spawn_y;
    loom_u8 hook_count;
    loom_u8 flags;
    loom_u8 sprite_slot;
    loom_u8 adventure_action;
    loom_u8 adventure_flag;
    loom_u8 adventure_request;
    loom_u8 adventure_gate_flag;
    /* For LOOM_ADVENTURE_ACTION_COLLECT the gate does not apply:
     * adventure_flag is the variable handle, adventure_request the sound's
     * request id, and this byte is the amount added. */
    loom_u8 adventure_gate_set;
} LoomSceneTrigger;

/* runtime/backends/pvsneslib/src/scene.asm walks the trigger array by hand,
 * the way the OAM path walks the sprite array. */
#define LOOM_SCENE_TRIGGER_BYTES 24
LOOM_STATIC_ASSERT(loom_scene_trigger_matches_its_stride,
                   sizeof(LoomSceneTrigger) == LOOM_SCENE_TRIGGER_BYTES);

struct LoomSurfaceScene;

struct LoomSurfaceScene;
struct LoomBoardScene;

typedef struct LoomSceneRecord {
    const LoomMode1Scene *mode1;
    const LoomMovementScene *movement;
    const LoomAnimationScene *animation;
    const LoomCameraScene *camera;
    /* Null when the scene places no actors. */
    const LoomActorScene *actors;
    const LoomSceneTrigger *triggers;
    const loom_u16 *hook_ids;
    loom_u16 trigger_count;
    loom_u16 hook_count;
    /* Trailing so older literals still zero-fill: the scene's surfaces
     * (SURF-001), or null when it authors none. */
    const struct LoomSurfaceScene *surfaces;
    /* The scene's boards (GRID-001), or null. */
    const struct LoomBoardScene *boards;
} LoomSceneRecord;

/* Defined by generated mode1_data.c. */
extern const loom_u8 loom_generated_scene_enabled;
extern const loom_u16 loom_generated_scene_count;
extern const loom_u16 loom_generated_scene_initial_index;
extern const loom_s16 loom_generated_scene_initial_spawn_x;
extern const loom_s16 loom_generated_scene_initial_spawn_y;
extern const LoomSceneRecord loom_generated_runtime_scenes[];

LoomStatus loom_scene_initialize(void);
LoomStatus loom_scene_load(loom_u16 scene_index,
                           loom_s16 spawn_x,
                           loom_s16 spawn_y);
LoomStatus loom_scene_move(const LoomInputSnapshot *input);
LoomStatus loom_scene_dispatch_triggers(const LoomFrameBoundary *boundary,
                                        const LoomInputSnapshot *input);
LoomStatus loom_scene_advance_animation(void);
LoomStatus loom_scene_update_camera(void);
loom_u16 loom_scene_index(void);
loom_u8 loom_scene_transition_phase(void);
loom_u16 loom_scene_transition_count(void);
loom_u16 loom_scene_trigger_dispatch_count(void);
/* Saturating count of collectibles taken since initialization (GAME-001). */
loom_u16 loom_scene_collect_count(void);
loom_u16 loom_scene_black_frame_count(void);
/* One call collects the scene fields the debug witness publishes. */
typedef struct LoomSceneDebugSnapshot {
    loom_u16 scene_index;
    loom_u16 transition_count;
    loom_u16 black_frame_count;
    loom_u16 trigger_dispatch_count;
    loom_u16 collect_count;
    loom_u16 last_transition_source;
    loom_u16 last_transition_target;
    loom_s16 last_transition_spawn_x;
    loom_s16 last_transition_spawn_y;
    loom_u8 transition_phase;
    loom_u8 player_slot;
} LoomSceneDebugSnapshot;
void loom_scene_debug_snapshot(LoomSceneDebugSnapshot *snapshot);
extern loom_u8 loom_scene_debug_epoch;
loom_u16 loom_scene_last_transition_source(void);
loom_u16 loom_scene_last_transition_target(void);
loom_s16 loom_scene_last_transition_spawn_x(void);
loom_s16 loom_scene_last_transition_spawn_y(void);
loom_u8 loom_scene_player_slot(void);

#endif
