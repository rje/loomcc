/* Loom 7b4d960^: runtime/include/loom/types.h and scene.h (LoomSceneTrigger),
 * runtime/src/scene.c (LoomSceneBox). */
#ifndef BENCH_LOOM_SCENE_H
#define BENCH_LOOM_SCENE_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

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
    loom_u8 adventure_gate_set;
} LoomSceneTrigger;

/* scene.asm steps 24 bytes per trigger. */
typedef char loom_scene_trigger_is_twenty_four_bytes
    [(sizeof(LoomSceneTrigger) == 24u) ? 1 : -1];

typedef struct LoomSceneBox {
    loom_s16 left;
    loom_s16 top;
    loom_s16 right;
    loom_s16 bottom;
} LoomSceneBox;

loom_u16 loom_pvs_scene_intersect_mask(const LoomSceneBox *player,
                                       const LoomSceneTrigger *triggers,
                                       loom_u8 count);

#endif
