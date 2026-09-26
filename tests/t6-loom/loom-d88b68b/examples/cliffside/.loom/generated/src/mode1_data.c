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
const loom_u16 loom_generated_combat_player_max_health = 8u;
const loom_u8 loom_generated_attack_enabled = LOOM_FALSE;
const loom_u8 loom_generated_attack_type_index = 0u;
const loom_u8 loom_generated_attack_launch_px = 0u;
const loom_u16 loom_generated_attack_button = 0x0000u;
const loom_u16 loom_generated_attack_cooldown_ticks = 0u;

const loom_u8 loom_generated_actor_type_count = 5u;
const LoomActorType loom_generated_actor_types[5] = {
    { 192u, 0, 0, 0u, 0u, 0u, 96u, 160u,
      0, 0, 0u, 0u, -6, -11, 12u, 8u,
      LOOM_ACTOR_BODY_TOP_DOWN, LOOM_ACTOR_COLLISION_NONE, LOOM_ACTOR_BEHAVIOR_CHASE, 0x00u, 0u, 1u, 0u, 0u, LOOM_ACTOR_INVALID_INDEX }, /* Cave Bat */
    { 256u, -8, -8, 16u, 8u, 300u, 0u, 0u,
      0, 0, 0u, 0u, 0, 0, 0u, 0u,
      LOOM_ACTOR_BODY_TOP_DOWN, LOOM_ACTOR_COLLISION_SOLID, LOOM_ACTOR_BEHAVIOR_PATROL, 0x01u, 0u, 0u, 0u, 0u, LOOM_ACTOR_INVALID_INDEX }, /* Ferry Platform */
    { 256u, -6, -10, 12u, 10u, 0u, 0u, 0u,
      0, 0, 0u, 0u, -6, -10, 12u, 10u,
      LOOM_ACTOR_BODY_PLATFORMER, LOOM_ACTOR_COLLISION_OVERLAP, LOOM_ACTOR_BEHAVIOR_BOUNCE, 0x02u, 0u, 1u, 0u, 0u, 0u }, /* Ledge Walker */
    { 512u, -6, -14, 12u, 14u, 2u, 0u, 0u,
      0, 0, 0u, 0u, 0, 0, 0u, 0u,
      LOOM_ACTOR_BODY_PLATFORMER, LOOM_ACTOR_COLLISION_OVERLAP, LOOM_ACTOR_BEHAVIOR_CONTROLLER, 0x00u, 0u, 0u, 0u, 0u, 1u }, /* Player Two */
    { 0u, 0, 0, 0u, 0u, 0u, 0u, 0u,
      0, 0, 0u, 0u, -7, -10, 14u, 8u,
      LOOM_ACTOR_BODY_NONE, LOOM_ACTOR_COLLISION_NONE, LOOM_ACTOR_BEHAVIOR_STATIC, 0x00u, 0u, 2u, 0u, 0u, LOOM_ACTOR_INVALID_INDEX }, /* Spike Trap */
};

const loom_u8 loom_generated_actor_body_count = 2u;
const LoomPlatformerBody loom_generated_actor_bodies[2] = {
    { 256u, 48u, 64u, 32u, 56u, 1536u, 1216u, 320u, 5u, 5u, 0x01u, 0u }, /* Ledge Walker */
    { 512u, 48u, 64u, 32u, 56u, 1536u, 1216u, 320u, 5u, 5u, 0x00u, 0u }, /* Player Two */
};

/* Scene 0c11f51de00000000000000000000040. */
static const LoomMode1LoadSegment loom_generated_scene_0_mode1_load_segments[8] = {
    { (LoomAssetHandle)3u, 0u, 0x2000u, 1440u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)10u, 0u, 0x4000u, 4096u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)8u, 0u, 0x0000u, 48u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)8u, 64u, 0x0040u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)6u, 0u, 0x0000u, 1792u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)7u, 0u, 0x0100u, 32u, LOOM_DMA_DESTINATION_CGRAM, 0u },
    { (LoomAssetHandle)5u, 0u, 0x6000u, 480u, LOOM_DMA_DESTINATION_VRAM, 0u },
    { (LoomAssetHandle)9u, 0u, 0x8000u, 4096u, LOOM_DMA_DESTINATION_VRAM, 0u },
};

static const LoomMode1Sprite loom_generated_scene_0_mode1_sprites[] = {
    { 40, 192, 8, 15, 0u, 0u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 300, 184, 8, 15, 2u, 1u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 470, 192, 8, 15, 4u, 2u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 790, 176, 8, 15, 4u, 3u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 700, 128, 8, 15, 6u, 4u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 584, 208, 8, 15, 8u, 5u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 856, 208, 8, 15, 8u, 6u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
    { 56, 192, 8, 15, 0u, 7u, 0u, 2u, LOOM_OAM_SIZE_LARGE, 0u, 16u, 16u, 0u, 0u },
};

static const LoomMode1Layer loom_generated_scene_0_mode1_layers[2] = {
    { 0u, 0u, 1u, 1u, 1u, 1u, 0, 0 }, /* Ground */
    { 1u, 0u, 1u, 2u, 0u, 1u, 0, 0 }, /* Backdrop */
};

/* Streamed BG1: 64x14 metatiles, 10 variants, window at column 0 row 0. */
static const loom_u8 loom_generated_scene_0_stream_grid[896] = {
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 0u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 1u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 2u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 
    255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 3u, 1u, 1u, 4u, 1u, 1u, 1u, 5u, 255u, 255u, 255u, 255u, 255u, 255u, 6u, 1u, 1u, 7u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 3u, 1u, 4u, 1u, 1u, 1u, 1u, 1u, 5u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 8u, 255u, 2u, 255u, 
    1u, 1u, 4u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 255u, 255u, 255u, 255u, 255u, 255u, 9u, 7u, 7u, 9u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 5u, 255u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 255u, 3u, 1u, 1u, 4u, 1u, 1u, 1u, 1u, 1u, 1u, 
    7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 255u, 255u, 255u, 255u, 255u, 255u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 1u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 1u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 7u, 9u, 7u, 
};

static const loom_u16 loom_generated_scene_0_stream_table[40] = {
    0x0007u, 0x0007u, 0x0004u, 0x0004u,
    0x0000u, 0x0001u, 0x000du, 0x000eu,
    0x0023u, 0x0024u, 0x0029u, 0x002au,
    0x0004u, 0x0005u, 0x0011u, 0x0012u,
    0x0009u, 0x000au, 0x000du, 0x000eu,
    0x0000u, 0x0001u, 0x000du, 0x0028u,
    0x0022u, 0x0001u, 0x0027u, 0x000eu,
    0x0002u, 0x0003u, 0x000fu, 0x0010u,
    0x000bu, 0x000cu, 0x0016u, 0x0017u,
    0x0025u, 0x0026u, 0x002bu, 0x002cu,
};

static const LoomMode1Stream loom_generated_scene_0_stream = {
    64u, 14u, 0u, 0u, 0x0004u, 10u, 0u,
    loom_generated_scene_0_stream_grid,
    loom_generated_scene_0_stream_table
};

static const loom_u8 loom_generated_scene_0_movement_collision_cells[896] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 2u, 2u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 
};

static const LoomPlatformerBody loom_generated_scene_0_movement_body = { 512u, 48u, 64u, 32u, 56u, 1536u, 1216u, 320u, 5u, 5u, 0x00u, 0u };

const LoomMovementScene loom_generated_movement_initial_scene = {
    1024u, 224u, 40, 192, -5, -14,
    10u, 14u, 512u, 64u, 14u, 0u, 2u,
    loom_generated_scene_0_movement_collision_cells, &loom_generated_scene_0_movement_body,
    0u, (const LoomStaticCollider *)0
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_frames[] = {
    { 8, 15, 0u, 12u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_0_slot_0[] = {
    { 8, 15, 0u, 12u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_1_slot_0[] = {
    { 8, 15, 10u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 12u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_2_slot_0[] = {
    { 8, 15, 14u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_3_slot_0[] = {
    { 8, 15, 32u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_0_state_4_slot_0[] = {
    { 8, 15, 34u, 4u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 0u, 2u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_0_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_0_slot_0 },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_1_slot_0 },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_2_slot_0 },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_0_state_3_slot_0 },
    { 2u, LOOM_FALSE, loom_generated_scene_0_animation_player_0_state_4_slot_0 },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_0_set = {
    1u, 1u, 5u, LOOM_ANIMATION_DRIVE_MOVEMENT, 0u, 1u, 2u, 3u, 4u,
    loom_generated_scene_0_animation_player_0_clips
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_1_frames[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_1_state_0_slot_0[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_1_state_1_slot_0[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_1_clips[] = {
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_1_state_0_slot_0 },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_1_state_1_slot_0 },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_1_set = {
    1u, 1u, 2u, LOOM_ANIMATION_DRIVE_NONE, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_0_animation_player_1_clips
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_2_frames[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_2_state_0_slot_0[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_2_state_1_slot_0[] = {
    { 8, 15, 4u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 36u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_2_clips[] = {
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_2_state_0_slot_0 },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_2_state_1_slot_0 },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_2_set = {
    1u, 1u, 2u, LOOM_ANIMATION_DRIVE_NONE, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_0_animation_player_2_clips
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_3_frames[] = {
    { 8, 15, 6u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 38u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_3_state_0_slot_0[] = {
    { 8, 15, 6u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 38u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_3_state_1_slot_0[] = {
    { 8, 15, 6u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 38u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_3_clips[] = {
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_3_state_0_slot_0 },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_3_state_1_slot_0 },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_3_set = {
    1u, 1u, 2u, LOOM_ANIMATION_DRIVE_NONE, 0u, 1u, 255u, 255u, 255u,
    loom_generated_scene_0_animation_player_3_clips
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_frames[] = {
    { 8, 15, 0u, 12u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_state_0_slot_0[] = {
    { 8, 15, 0u, 12u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_state_1_slot_0[] = {
    { 8, 15, 10u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 12u, 6u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_state_2_slot_0[] = {
    { 8, 15, 14u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_state_3_slot_0[] = {
    { 8, 15, 32u, 8u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationFrame loom_generated_scene_0_animation_player_4_state_4_slot_0[] = {
    { 8, 15, 34u, 4u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
    { 8, 15, 0u, 2u, 0u, LOOM_OAM_SIZE_LARGE, 16u, 16u },
};

static const LoomAnimationClip loom_generated_scene_0_animation_player_4_clips[] = {
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_4_state_0_slot_0 },
    { 2u, LOOM_TRUE, loom_generated_scene_0_animation_player_4_state_1_slot_0 },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_4_state_2_slot_0 },
    { 1u, LOOM_TRUE, loom_generated_scene_0_animation_player_4_state_3_slot_0 },
    { 2u, LOOM_FALSE, loom_generated_scene_0_animation_player_4_state_4_slot_0 },
};

static const LoomAnimationSet loom_generated_scene_0_animation_player_4_set = {
    1u, 1u, 5u, LOOM_ANIMATION_DRIVE_NONE, 0u, 1u, 2u, 3u, 4u,
    loom_generated_scene_0_animation_player_4_clips
};

static const LoomAnimationPlayer loom_generated_scene_0_animation_players[] = {
    { 0u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_0_frames, &loom_generated_scene_0_animation_player_0_set },
    { 2u, 2u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_1_frames, &loom_generated_scene_0_animation_player_1_set },
    { 3u, 2u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_2_frames, &loom_generated_scene_0_animation_player_2_set },
    { 4u, 2u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_3_frames, &loom_generated_scene_0_animation_player_3_set },
    { 7u, 1u, LOOM_TRUE, LOOM_TRUE, loom_generated_scene_0_animation_player_4_frames, &loom_generated_scene_0_animation_player_4_set },
};

const LoomAnimationScene loom_generated_animation_initial_scene = {
    5u, 0u, loom_generated_scene_0_animation_players
};

static const LoomActorWaypoint loom_generated_scene_0_actor_waypoints[2] = {
    { 300, 184 },
    { 376, 184 },
};

static const LoomActorInstance loom_generated_scene_0_actor_instances[7] = {
    { 300, 184, 0u, 2u, 1u, 1u, 0u },
    { 470, 192, 2u, 0u, 2u, 2u, 0u },
    { 790, 176, 2u, 0u, 2u, 3u, 0u },
    { 700, 128, 2u, 0u, 0u, 4u, 0u },
    { 584, 208, 2u, 0u, 4u, 5u, 0u },
    { 856, 208, 2u, 0u, 4u, 6u, 0u },
    { 56, 192, 2u, 0u, 3u, 7u, 0u },
};

const LoomActorScene loom_generated_actor_initial_scene = {
    7u, 0u,
    loom_generated_scene_0_actor_instances,
    loom_generated_scene_0_actor_waypoints
};

static const LoomCameraRegion loom_generated_scene_0_camera_regions[2] = {
    { 640, 0, 128u, 224u, { 0u, 0u, 0u, 0u, 0u, 0u, 128, 0 } },
    { 768, 0, 256u, 224u, { 2u, 0u, 0u, 0u, 0u, 0u, 0, 0 } },
};

const LoomCameraScene loom_generated_camera_initial_scene = {
    128, 112, 0u, 2u, { 1u, 0u, 48u, 32u, 0u, 0u, 0, 0 }, loom_generated_scene_0_camera_regions,
    1u, 0u
};

static const LoomMode1RasterParams loom_generated_scene_0_raster_params = {
    LOOM_MODE1_RASTER_BACKDROP_GRADIENT, 0u, 0u, 0u, 0u, 0, 0u, 0u,
    { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
    { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u },
    { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u }
};
const LoomMode1Scene loom_generated_mode1_initial_scene = {
    1024u, 224u,
    0, 0, 768, 0,
    0, 0,
    0x726bu, { LOOM_GENERATED_RASTER_PROGRAM_0, LOOM_RASTER_STATE_NONE }, LOOM_OBJ_SIZE_8_16,
    8u, 8u, 0u,
    loom_generated_scene_0_mode1_load_segments,
    loom_generated_scene_0_mode1_sprites,
    2u, 0u, loom_generated_scene_0_mode1_layers,
    &loom_generated_scene_0_stream,
    0u, 0u, (const LoomMode1TileAnimation *)0, (const LoomMode1PaletteCycle *)0,
    &loom_generated_scene_0_raster_params, 0u, 0u, 0u, 0u
};

const loom_u8 loom_generated_ui_enabled = LOOM_TRUE;
const LoomUiCatalog loom_generated_ui_catalog = { LOOM_INVALID_HANDLE, LOOM_INVALID_HANDLE, 0u, 0u, 0u, LOOM_UI_SCREEN_NONE, (const LoomUiScreen *)0 };

const loom_u8 loom_generated_scene_enabled = LOOM_TRUE;
const loom_u16 loom_generated_scene_count = 1u;
const loom_u16 loom_generated_scene_initial_index = 0u;
const loom_s16 loom_generated_scene_initial_spawn_x = 40;
const loom_s16 loom_generated_scene_initial_spawn_y = 192;

const LoomSceneRecord loom_generated_runtime_scenes[] = {
    { &loom_generated_mode1_initial_scene, &loom_generated_movement_initial_scene, &loom_generated_animation_initial_scene, &loom_generated_camera_initial_scene, &loom_generated_actor_initial_scene, (const LoomSceneTrigger *)0, (const loom_u16 *)0, 0u, 0u },
};
