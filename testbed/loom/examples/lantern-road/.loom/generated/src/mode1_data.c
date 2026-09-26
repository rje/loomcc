/* Generated fixed room resources, triggers, hooks, UI, and transition graph. */
#include <loom/scene.h>
#include <loom/ui.h>
#include <loom/generated/raster_programs.h>

/* Keep byte flags first: 816-tcc treats a far pointer with offset $0000 as null. */
const loom_u8 loom_generated_mode1_enabled = LOOM_TRUE;
const loom_u8 loom_generated_movement_enabled = LOOM_TRUE;
const loom_u8 loom_generated_animation_enabled = LOOM_TRUE;
const loom_u8 loom_generated_camera_enabled = LOOM_TRUE;
const loom_u8 loom_generated_actors_enabled = LOOM_TRUE;
const loom_u8 loom_generated_combat_enabled = LOOM_TRUE;
const loom_u8 loom_generated_combat_player_invulnerable_ticks = 45u;
const loom_u16 loom_generated_combat_player_max_health = 6u;
const loom_u8 loom_generated_attack_enabled = LOOM_TRUE;
const loom_u8 loom_generated_attack_type_index = 1u;
const loom_u8 loom_generated_attack_launch_px = 10u;
const loom_u16 loom_generated_attack_button = 0x8000u;
const loom_u16 loom_generated_attack_cooldown_ticks = 18u;

const loom_u8 loom_generated_actor_type_count = 3u;
const LoomActorType loom_generated_actor_types[3] = {
    { 256u, -8, -8, 16u, 16u, 8u, 0u, 0u,
      -8, -8, 16u, 16u, -8, -8, 16u, 16u,
      LOOM_ACTOR_BODY_TOP_DOWN, LOOM_ACTOR_COLLISION_OVERLAP, LOOM_ACTOR_BEHAVIOR_PATROL, 0x01u, 3u, 2u, 4u, 45u, LOOM_ACTOR_INVALID_INDEX }, /* Grove Wisp */
    { 768u, 0, 0, 0u, 0u, 40u, 0u, 0u,
      0, 0, 0u, 0u, -4, -4, 8u, 8u,
      LOOM_ACTOR_BODY_TOP_DOWN, LOOM_ACTOR_COLLISION_NONE, LOOM_ACTOR_BEHAVIOR_PROJECTILE, 0x00u, 0u, 1u, 0u, 0u, LOOM_ACTOR_INVALID_INDEX }, /* Lantern Spark */
    { 0u, -8, -8, 16u, 16u, 0u, 0u, 0u,
      0, 0, 0u, 0u, 0, 0, 0u, 0u,
      LOOM_ACTOR_BODY_NONE, LOOM_ACTOR_COLLISION_SOLID, LOOM_ACTOR_BEHAVIOR_STATIC, 0x00u, 0u, 0u, 0u, 0u, LOOM_ACTOR_INVALID_INDEX }, /* Wayside Stone */
};

const loom_u8 loom_generated_actor_body_count = 0u;
const LoomPlatformerBody loom_generated_actor_bodies[1] = { { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u } };

/* Scene 0000000000006f1c18c0da4fd2d10328. */
static const LoomMode1LoadSegment loom_generated_scene_0_mode1_load_segments[6] = {
    { (LoomAssetHandle)16u, 0u, 0x2000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)17u, 0u, 0x4000u, 2048u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)4u, 0u, 0x0000u, 48u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)4u, 64u, 0x0040u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)14u, 0u, 0x0000u, 2944u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)15u, 0u, 0x0100u, 128u, LOOM_DMA_DESTINATION_CGRAM, 0u },
};

static const LoomMode1Sprite loom_generated_scene_0_mode1_sprites[] = {
    { 128, 112, 8, 15, 0u, 0u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 192, 112, 8, 15, 2u, 1u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 192, 112, 8, 15, 4u, 2u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 3u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 4u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
};

static const LoomMode1Layer loom_generated_scene_0_mode1_layers[1] = {
    { 0u, 0u, 1u, 1u, 1u, 1u, 0, 0 }, /* BG1 Ground */
};

static const loom_u8 loom_generated_scene_0_movement_collision_cells[224] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
};

static const LoomMovementScene loom_generated_scene_0_movement = {
    256u, 224u, 128, 112, -8, -8,
    16u, 16u, 256u, 16u, 14u, 0u, 1u,
    loom_generated_scene_0_movement_collision_cells, (const LoomPlatformerBody *)0,
    0u, (const LoomStaticCollider *)0
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_frames[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_0_slot_0[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_0_slot_1[] = {
    { 8, 15, 64u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_0_slot_2[] = {
    { 8, 15, 66u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_0_slot_3[] = {
    { 8, 15, 68u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_1_slot_0[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_1_slot_1[] = {
    { 8, 15, 64u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 70u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_1_slot_2[] = {
    { 8, 15, 66u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 72u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_1_slot_3[] = {
    { 8, 15, 68u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 74u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_0_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_0_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_0_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_0_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_0_slot_3, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_1_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_1_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_1_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_1_slot_3, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_0_set = {
    3u, 4u, 2u, LOOM_ANIMATION_DRIVE_MOVEMENT, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_0_animation_player_0_clips
};

static const LoomAnimationPlayer loom_generated_scene_0_animation_players[] = {
    { 0u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_0_frames, &loom_generated_scene_0_animation_player_0_set, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationScene loom_generated_scene_0_animation = {
    1u, 0u, loom_generated_scene_0_animation_players
};

static const LoomActorInstance loom_generated_scene_0_actor_instances[2] = {
    { 0, 0, 0u, 0u, 1u, 3u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
    { 0, 0, 0u, 0u, 1u, 4u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
};

static const LoomActorScene loom_generated_scene_0_actors = {
    2u, 0u,
    loom_generated_scene_0_actor_instances,
    (const LoomActorWaypoint *)0
};

static const LoomCameraScene loom_generated_scene_0_camera = {
    128, 112, 0u, 0u, { 0u, 0u, 0u, 0u, 0u, 0u, 0, 0 }, (const LoomCameraRegion *)0,
    0u, 0u
};

static const LoomMode1Scene loom_generated_scene_0_mode1 = {
    256u, 224u,
    0, 0, 0, 0,
    0, 0,
    0x14c7u, { LOOM_RASTER_PROGRAM_NONE, LOOM_RASTER_STATE_NONE }, LOOM_OBJ_SIZE_8_16,
    6u, 5u, 0u,
    loom_generated_scene_0_mode1_load_segments,
    loom_generated_scene_0_mode1_sprites,
    1u, 0u, loom_generated_scene_0_mode1_layers,
    (const LoomMode1Stream *)0,
    0u, 0u, (const LoomMode1TileAnimation *)0, (const LoomMode1PaletteCycle *)0,
    (const LoomMode1RasterParams *)0, 0u, 0u, 0u, 0u
};

/* Scene 0000000000006f1c18c4da4c9c8c99b0. */
static const LoomMode1LoadSegment loom_generated_scene_1_mode1_load_segments[8] = {
    { (LoomAssetHandle)16u, 0u, 0x2000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)20u, 0u, 0x4000u, 4096u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)18u, 0u, 0x0000u, 48u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)18u, 64u, 0x0040u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)14u, 0u, 0x0000u, 2944u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)15u, 0u, 0x0100u, 128u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)16u, 0u, 0x6000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)19u, 0u, 0x8000u, 4096u, LOOM_DMA_DESTINATION_VRAM, 0u },
};

static const LoomMode1Sprite loom_generated_scene_1_mode1_sprites[] = {
    { 128, 112, 8, 15, 0u, 0u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 448, 112, 8, 15, 6u, 1u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 448, 112, 8, 15, 8u, 2u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 3u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 4u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
};

static const LoomMode1Layer loom_generated_scene_1_mode1_layers[2] = {
    { 0u, 0u, 1u, 1u, 1u, 1u, 0, 0 }, /* BG1 Ground */
    { 1u, 0u, 1u, 2u, 1u, 1u, 0, 0 }, /* Sky */
};

static const loom_u8 loom_generated_scene_1_movement_collision_cells[448] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
};

static const LoomMovementScene loom_generated_scene_1_movement = {
    512u, 224u, 128, 112, -8, -8,
    16u, 16u, 256u, 32u, 14u, 0u, 1u,
    loom_generated_scene_1_movement_collision_cells, (const LoomPlatformerBody *)0,
    0u, (const LoomStaticCollider *)0
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_frames[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_0_slot_0[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_0_slot_1[] = {
    { 8, 15, 64u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_0_slot_2[] = {
    { 8, 15, 66u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_0_slot_3[] = {
    { 8, 15, 68u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_1_slot_0[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_1_slot_1[] = {
    { 8, 15, 64u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 70u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_1_slot_2[] = {
    { 8, 15, 66u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 72u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_1_animation_player_0_state_1_slot_3[] = {
    { 8, 15, 68u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 74u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_1_animation_player_0_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_0_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_0_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_0_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_0_slot_3, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_1_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_1_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_1_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_1_animation_player_0_state_1_slot_3, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationSet loom_generated_scene_1_animation_player_0_set = {
    3u, 4u, 2u, LOOM_ANIMATION_DRIVE_MOVEMENT, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_1_animation_player_0_clips
};

static const LoomAnimationPlayer loom_generated_scene_1_animation_players[] = {
    { 0u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_1_animation_player_0_frames, &loom_generated_scene_1_animation_player_0_set, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationScene loom_generated_scene_1_animation = {
    1u, 0u, loom_generated_scene_1_animation_players
};

static const LoomActorInstance loom_generated_scene_1_actor_instances[2] = {
    { 0, 0, 0u, 0u, 1u, 3u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
    { 0, 0, 0u, 0u, 1u, 4u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
};

static const LoomActorScene loom_generated_scene_1_actors = {
    2u, 0u,
    loom_generated_scene_1_actor_instances,
    (const LoomActorWaypoint *)0
};

static const LoomCameraScene loom_generated_scene_1_camera = {
    128, 112, 0u, 0u, { 0u, 0u, 0u, 0u, 0u, 0u, 0, 0 }, (const LoomCameraRegion *)0,
    0u, 0u
};

static const LoomMode1Scene loom_generated_scene_1_mode1 = {
    512u, 224u,
    0, 0, 256, 0,
    0, 0,
    0x14c7u, { LOOM_RASTER_PROGRAM_NONE, LOOM_RASTER_STATE_NONE }, LOOM_OBJ_SIZE_8_16,
    8u, 5u, 0u,
    loom_generated_scene_1_mode1_load_segments,
    loom_generated_scene_1_mode1_sprites,
    2u, 0u, loom_generated_scene_1_mode1_layers,
    (const LoomMode1Stream *)0,
    0u, 0u, (const LoomMode1TileAnimation *)0, (const LoomMode1PaletteCycle *)0,
    (const LoomMode1RasterParams *)0, 0u, 0u, 0u, 0u
};

/* Scene 0000000000006f1c18c8da486e7bf858. */
static const LoomMode1LoadSegment loom_generated_scene_2_mode1_load_segments[8] = {
    { (LoomAssetHandle)16u, 0u, 0x2000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)23u, 0u, 0x4000u, 2048u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)4u, 0u, 0x0000u, 48u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)4u, 64u, 0x0040u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)14u, 0u, 0x0000u, 2944u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)15u, 0u, 0x0100u, 128u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)16u, 0u, 0x6000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)22u, 0u, 0x8000u, 2048u, LOOM_DMA_DESTINATION_VRAM, 0u },
};

static const LoomMode1Sprite loom_generated_scene_2_mode1_sprites[] = {
    { 128, 112, 8, 15, 0u, 0u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 96, 200, 8, 15, 0u, 1u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 128, 176, 8, 15, 10u, 2u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 3u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 4u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
};

static const LoomMode1Layer loom_generated_scene_2_mode1_layers[2] = {
    { 0u, 0u, 1u, 1u, 1u, 1u, 0, 0 }, /* BG1 Ground */
    { 1u, 0u, 1u, 1u, 1u, 1u, 24, 0 }, /* Mist */
};

static const loom_u8 loom_generated_scene_2_movement_collision_cells[224] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
};

static const LoomMovementScene loom_generated_scene_2_movement = {
    256u, 224u, 128, 112, -8, -8,
    16u, 16u, 256u, 16u, 14u, 0u, 1u,
    loom_generated_scene_2_movement_collision_cells, (const LoomPlatformerBody *)0,
    0u, (const LoomStaticCollider *)0
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_frames[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_0_slot_0[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_0_slot_1[] = {
    { 8, 15, 64u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_0_slot_2[] = {
    { 8, 15, 66u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_0_slot_3[] = {
    { 8, 15, 68u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_1_slot_0[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_1_slot_1[] = {
    { 8, 15, 64u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 70u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_1_slot_2[] = {
    { 8, 15, 66u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 72u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_0_state_1_slot_3[] = {
    { 8, 15, 68u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 74u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_2_animation_player_0_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_0_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_0_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_0_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_0_slot_3, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_1_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_1_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_1_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_0_state_1_slot_3, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationSet loom_generated_scene_2_animation_player_0_set = {
    3u, 4u, 2u, LOOM_ANIMATION_DRIVE_MOVEMENT, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_2_animation_player_0_clips
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_frames[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_0_slot_0[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_0_slot_1[] = {
    { 8, 15, 64u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_0_slot_2[] = {
    { 8, 15, 66u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_0_slot_3[] = {
    { 8, 15, 68u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_1_slot_0[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_1_slot_1[] = {
    { 8, 15, 64u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 70u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_1_slot_2[] = {
    { 8, 15, 66u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 72u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_2_animation_player_1_state_1_slot_3[] = {
    { 8, 15, 68u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 74u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_2_animation_player_1_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_0_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_0_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_0_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_0_slot_3, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_1_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_1_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_1_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_2_animation_player_1_state_1_slot_3, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationSet loom_generated_scene_2_animation_player_1_set = {
    3u, 4u, 2u, LOOM_ANIMATION_DRIVE_NONE, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_2_animation_player_1_clips
};

static const LoomAnimationPlayer loom_generated_scene_2_animation_players[] = {
    { 0u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_2_animation_player_0_frames, &loom_generated_scene_2_animation_player_0_set, (const LoomMode1SpritePose *)0, 0u },
    { 1u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_2_animation_player_1_frames, &loom_generated_scene_2_animation_player_1_set, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationScene loom_generated_scene_2_animation = {
    2u, 0u, loom_generated_scene_2_animation_players
};

static const LoomActorWaypoint loom_generated_scene_2_actor_waypoints[2] = {
    { 96, 200 },
    { 160, 200 },
};

static const LoomActorInstance loom_generated_scene_2_actor_instances[4] = {
    { 96, 200, 0u, 2u, 0u, 1u, 0u },
    { 128, 176, 2u, 0u, 2u, 2u, 0u },
    { 0, 0, 0u, 0u, 1u, 3u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
    { 0, 0, 0u, 0u, 1u, 4u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
};

static const LoomActorScene loom_generated_scene_2_actors = {
    4u, 0u,
    loom_generated_scene_2_actor_instances,
    loom_generated_scene_2_actor_waypoints
};

static const LoomCameraScene loom_generated_scene_2_camera = {
    128, 112, 0u, 0u, { 0u, 0u, 0u, 0u, 0u, 0u, 0, 0 }, (const LoomCameraRegion *)0,
    0u, 0u
};

static const loom_u16 loom_generated_scene_2_tile_animation_0_durations[3] = { 12u, 12u, 12u };
static const LoomMode1TileRun loom_generated_scene_2_tile_animation_0_runs[2] = {
    { 0u, 0x2680u, 32u, 0u },
    { 32u, 0x2720u, 32u, 0u },
};
static const LoomMode1TileAnimation loom_generated_scene_2_tile_animations[1] = {
    { (LoomAssetHandle)21u, 64u, 3u, 2u, loom_generated_scene_2_tile_animation_0_durations, loom_generated_scene_2_tile_animation_0_runs },
};

static const LoomMode1Scene loom_generated_scene_2_mode1 = {
    256u, 224u,
    0, 0, 0, 0,
    0, 0,
    0x14c7u, { LOOM_RASTER_PROGRAM_NONE, LOOM_RASTER_STATE_NONE }, LOOM_OBJ_SIZE_8_16,
    8u, 5u, 0u,
    loom_generated_scene_2_mode1_load_segments,
    loom_generated_scene_2_mode1_sprites,
    2u, 0u, loom_generated_scene_2_mode1_layers,
    (const LoomMode1Stream *)0,
    1u, 0u, loom_generated_scene_2_tile_animations, (const LoomMode1PaletteCycle *)0,
    (const LoomMode1RasterParams *)0, 2u, 1u, 0u, 0u
};

/* Scene 0000000000006f1c18cf5a25afdf89e0. */
static const LoomMode1LoadSegment loom_generated_scene_3_mode1_load_segments[6] = {
    { (LoomAssetHandle)16u, 0u, 0x2000u, 2016u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)24u, 0u, 0x4000u, 2048u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)4u, 0u, 0x0000u, 48u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)4u, 64u, 0x0040u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)14u, 0u, 0x0000u, 2944u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)15u, 0u, 0x0100u, 128u, LOOM_DMA_DESTINATION_CGRAM, 0u },
};

static const LoomMode1Sprite loom_generated_scene_3_mode1_sprites[] = {
    { 128, 112, 8, 15, 0u, 0u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 80, 96, 8, 15, 0u, 1u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 160, 112, 8, 15, 12u, 2u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 200, 112, 8, 15, 14u, 3u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 224, 96, 8, 15, 32u, 4u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 224, 96, 8, 15, 10u, 5u, 1u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 112, 16, 16, 34u, 6u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 5u, 0u },
    { 56, 112, 16, 48, 36u, 7u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 112, 0, 48, 38u, 8u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 112, 16, 32, 40u, 9u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 112, 0, 32, 42u, 10u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 112, 0, 16, 44u, 11u, 2u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 12u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
    { 128, 112, 4, 4, 76u, 13u, 3u, 2u, LOOM_OAM_SIZE_SMALL, 0u, 8u, 8u, 0u, 0u },
};

static const LoomMode1Layer loom_generated_scene_3_mode1_layers[1] = {
    { 0u, 0u, 1u, 1u, 1u, 1u, 0, 0 }, /* BG1 Ground */
};

static const loom_u8 loom_generated_scene_3_movement_collision_cells[224] = {
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 
};

static const LoomStaticCollider loom_generated_scene_3_static_colliders[1] = {
    { 40, 96, 71, 119 },
};

const LoomMovementScene loom_generated_movement_initial_scene = {
    256u, 224u, 128, 112, -8, -8,
    16u, 16u, 256u, 16u, 14u, 0u, 1u,
    loom_generated_scene_3_movement_collision_cells, (const LoomPlatformerBody *)0,
    1u, loom_generated_scene_3_static_colliders
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_frames[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_0_slot_0[] = {
    { 8, 15, 0u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_0_slot_1[] = {
    { 8, 15, 64u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_0_slot_2[] = {
    { 8, 15, 66u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_0_slot_3[] = {
    { 8, 15, 68u, 30u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_1_slot_0[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_1_slot_1[] = {
    { 8, 15, 64u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 70u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_1_slot_2[] = {
    { 8, 15, 66u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 72u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_0_state_1_slot_3[] = {
    { 8, 15, 68u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 74u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_3_animation_player_0_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_0_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_0_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_0_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 1u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_0_slot_3, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_1_slot_0, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_1_slot_1, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_1_slot_2, (const LoomMode1SpritePose *)0, 0u },
    { 2u, LOOM_TRUE, loom_generated_scene_3_animation_player_0_state_1_slot_3, (const LoomMode1SpritePose *)0, 0u },
};

static const LoomAnimationSet loom_generated_scene_3_animation_player_0_set = {
    3u, 4u, 2u, LOOM_ANIMATION_DRIVE_MOVEMENT, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_3_animation_player_0_clips
};

static const LoomAnimationFrame loom_generated_scene_3_animation_player_1_frames[] = {
    { 8, 15, 0u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 46u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationPlayer loom_generated_scene_3_animation_players[] = {
    { 0u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_3_animation_player_0_frames, &loom_generated_scene_3_animation_player_0_set, (const LoomMode1SpritePose *)0, 0u },
    { 1u, 2u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_3_animation_player_1_frames, (const LoomAnimationSet *)0, (const LoomMode1SpritePose *)0, 0u },
};

const LoomAnimationScene loom_generated_animation_initial_scene = {
    2u, 0u, loom_generated_scene_3_animation_players
};

static const LoomActorInstance loom_generated_scene_3_actor_instances[2] = {
    { 0, 0, 0u, 0u, 1u, 12u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
    { 0, 0, 0u, 0u, 1u, 13u, LOOM_ACTOR_INSTANCE_FLAG_POOLED },
};

const LoomActorScene loom_generated_actor_initial_scene = {
    2u, 0u,
    loom_generated_scene_3_actor_instances,
    (const LoomActorWaypoint *)0
};

const LoomCameraScene loom_generated_camera_initial_scene = {
    128, 112, 0u, 0u, { 0u, 0u, 0u, 0u, 0u, 0u, 0, 0 }, (const LoomCameraRegion *)0,
    0u, 0u
};

static const LoomMode1RasterParams loom_generated_scene_3_raster_params = {
    LOOM_MODE1_RASTER_FIXED_COLOR, 0u, 0u, 0u, 0u, 0, 0u, 0u,
    { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
    { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
    { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u }
};
const LoomMode1Scene loom_generated_mode1_initial_scene = {
    256u, 224u,
    0, 0, 0, 0,
    0, 0,
    0x14c7u, { LOOM_GENERATED_RASTER_PROGRAM_0, LOOM_RASTER_STATE_NONE }, LOOM_OBJ_SIZE_8_16,
    6u, 14u, 0u,
    loom_generated_scene_3_mode1_load_segments,
    loom_generated_scene_3_mode1_sprites,
    1u, 0u, loom_generated_scene_3_mode1_layers,
    (const LoomMode1Stream *)0,
    0u, 0u, (const LoomMode1TileAnimation *)0, (const LoomMode1PaletteCycle *)0,
    &loom_generated_scene_3_raster_params, 0u, 0u, 0u, 0u
};

const loom_u8 loom_generated_ui_enabled = LOOM_TRUE;
static const LoomUiScreen loom_generated_ui_screens[] = {
    { (LoomAssetHandle)27u, 1152u, 0u, 0x0008u, 0x0000u, 0x0000u, LOOM_UI_SCREEN_BLOCKS_GAMEPLAY, 0u },
    { (LoomAssetHandle)28u, 320u, 0u, 0x0000u, 0x0008u, 0x0000u, 0u, 1u },
    { (LoomAssetHandle)29u, 1152u, 1u, 0x0000u, 0x0004u, 0x0000u, 0u, 0u },
    { (LoomAssetHandle)30u, 320u, 1u, 0x0004u, 0x0000u, 0x0000u, 0u, 1u },
    { (LoomAssetHandle)31u, 1152u, 2u, 0x0002u, 0x0000u, 0x0000u, 0u, 0u },
    { (LoomAssetHandle)32u, 1152u, 3u, 0x0000u, 0x0000u, 0x1000u, (loom_u8)(LOOM_UI_SCREEN_INITIAL | LOOM_UI_SCREEN_BLOCKS_GAMEPLAY), 0u },
    { (LoomAssetHandle)33u, 320u, 3u, 0x0000u, 0x0001u, 0x0000u, 0u, 1u },
    { (LoomAssetHandle)34u, 256u, 3u, 0x0001u, 0x0002u, 0x0000u, 0u, 2u },
    { (LoomAssetHandle)35u, 256u, 3u, 0x0002u, 0x0000u, 0x0000u, 0u, 3u },
};

const LoomUiCatalog loom_generated_ui_catalog = {
    (LoomAssetHandle)25u, (LoomAssetHandle)26u,
    2112u, 64u, 9u, 5u,
    loom_generated_ui_screens
};

static const LoomSceneTrigger loom_generated_scene_0_triggers[] = {
    { 240, 96, 16u, 32u, 0u, 65535u, 0, 0, 0u, 0u, 255u, 0u, 255u, 255u, 255u, 0u },
    { 184, 104, 16u, 16u, 0u, 65535u, 0, 0, 0u, 0u, 1u, 2u, 255u, 11u, 3u, 0u },
    { 184, 104, 16u, 16u, 0u, 65535u, 0, 0, 0u, 0u, 2u, 0u, 255u, 255u, 3u, 1u },
};

static const LoomSceneTrigger loom_generated_scene_1_triggers[] = {
    { 496, 96, 16u, 32u, 0u, 0u, 128, 112, 0u, 2u, 255u, 0u, 255u, 255u, 2u, 1u },
    { 440, 104, 16u, 16u, 0u, 65535u, 0, 0, 0u, 0u, 1u, 2u, 255u, 10u, 2u, 0u },
    { 440, 104, 16u, 16u, 0u, 65535u, 0, 0, 0u, 0u, 2u, 0u, 255u, 255u, 2u, 1u },
};

static const LoomSceneTrigger loom_generated_scene_2_triggers[] = {
    { 240, 96, 16u, 32u, 0u, 1u, 128, 112, 0u, 2u, 255u, 0u, 255u, 255u, 255u, 0u },
};

static const loom_u16 loom_generated_scene_3_hook_ids[] = {
    42035u, 
};

static const LoomSceneTrigger loom_generated_scene_3_triggers[] = {
    { 240, 96, 16u, 32u, 0u, 2u, 128, 112, 0u, 2u, 255u, 0u, 255u, 255u, 1u, 1u },
    { 120, 104, 16u, 16u, 0u, 65535u, 0, 0, 1u, 1u, 255u, 0u, 255u, 255u, 255u, 0u },
    { 152, 104, 16u, 16u, 1u, 65535u, 0, 0, 0u, 1u, 2u, 1u, 0u, 7u, 0u, 0u },
    { 192, 104, 16u, 16u, 1u, 65535u, 0, 0, 0u, 0u, 3u, 2u, 255u, 9u, 0u, 1u },
    { 216, 88, 16u, 16u, 1u, 65535u, 0, 0, 0u, 0u, 4u, 0u, 255u, 255u, 1u, 1u },
    { 216, 88, 16u, 16u, 1u, 65535u, 0, 0, 0u, 0u, 5u, 0u, 255u, 255u, 1u, 0u },
};

const loom_u8 loom_generated_scene_enabled = LOOM_TRUE;
const loom_u16 loom_generated_scene_count = 4u;
const loom_u16 loom_generated_scene_initial_index = 3u;
const loom_s16 loom_generated_scene_initial_spawn_x = 128;
const loom_s16 loom_generated_scene_initial_spawn_y = 112;

const LoomSceneRecord loom_generated_runtime_scenes[] = {
    { &loom_generated_scene_0_mode1, &loom_generated_scene_0_movement, &loom_generated_scene_0_animation, &loom_generated_scene_0_camera, &loom_generated_scene_0_actors, loom_generated_scene_0_triggers, (const loom_u16 *)0, 3u, 0u, (const struct LoomSurfaceScene *)0, (const struct LoomBoardScene *)0 },
    { &loom_generated_scene_1_mode1, &loom_generated_scene_1_movement, &loom_generated_scene_1_animation, &loom_generated_scene_1_camera, &loom_generated_scene_1_actors, loom_generated_scene_1_triggers, (const loom_u16 *)0, 3u, 0u, (const struct LoomSurfaceScene *)0, (const struct LoomBoardScene *)0 },
    { &loom_generated_scene_2_mode1, &loom_generated_scene_2_movement, &loom_generated_scene_2_animation, &loom_generated_scene_2_camera, &loom_generated_scene_2_actors, loom_generated_scene_2_triggers, (const loom_u16 *)0, 1u, 0u, (const struct LoomSurfaceScene *)0, (const struct LoomBoardScene *)0 },
    { &loom_generated_mode1_initial_scene, &loom_generated_movement_initial_scene, &loom_generated_animation_initial_scene, &loom_generated_camera_initial_scene, &loom_generated_actor_initial_scene, loom_generated_scene_3_triggers, loom_generated_scene_3_hook_ids, 6u, 1u, (const struct LoomSurfaceScene *)0, (const struct LoomBoardScene *)0 },
};
