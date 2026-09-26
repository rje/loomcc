#ifndef LOOM_PROJECT_DEBUG_H
#define LOOM_PROJECT_DEBUG_H

#include <loom/animation.h>
#include <loom/adventure.h>
#include <loom/audio.h>
#include <loom/runtime.h>
#include <loom/ui.h>

#define LOOM_PROJECT_DEBUG_SCHEMA_VERSION ((loom_u16)9u)
#define LOOM_PROJECT_DEBUG_SCENE_CAPACITY ((loom_u8)2u)

#define LOOM_PROJECT_DEBUG_PHASE_FADE_OUT ((loom_u16)0x0001u)
#define LOOM_PROJECT_DEBUG_PHASE_BLACK_WAIT ((loom_u16)0x0002u)
#define LOOM_PROJECT_DEBUG_PHASE_RESOURCE_SWAP ((loom_u16)0x0004u)
#define LOOM_PROJECT_DEBUG_PHASE_FADE_IN ((loom_u16)0x0008u)
#define LOOM_PROJECT_DEBUG_PHASE_FINAL_WAIT ((loom_u16)0x0010u)

/*
 * Stable, backend-neutral state published only by debug cartridge builds.
 * ROM tests use this compact witness instead of compiler-private symbols.
 * Scene identities are generated zero-based ordinals; audio identities remain
 * the same stable Loom cue handles used by portable game code.
 */
typedef struct LoomProjectDebugState {
    loom_u8 magic[4];
    loom_u16 schema_version;
    loom_u16 logical_tick_count;
    LoomFrameId frame_id;
    loom_u16 room_index;
    loom_u16 room_ready;
    loom_u16 room_enter_count[LOOM_PROJECT_DEBUG_SCENE_CAPACITY];
    loom_s16 player_x;
    loom_s16 player_y;
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u16 blocked_movement_count;
    loom_u16 transition_count;
    loom_u16 transition_epoch;
    loom_u16 transition_seen_mask;
    loom_u16 black_frame_count;
    loom_u16 trigger_dispatch_count;
    LoomAudioCueHandle current_music_cue;
    LoomAudioCueHandle last_audio_cue;
    loom_u16 music_start_count;
    loom_u16 sfx_dispatch_count;
    loom_u16 missed_commit_count;
    loom_u16 mixed_resource_presented_frame_count;
    loom_u16 last_transition_source_scene;
    loom_u16 last_transition_target_scene;
    loom_s16 last_transition_spawn_x;
    loom_s16 last_transition_spawn_y;
    loom_u8 player_animation_frame;
    loom_u8 last_collision_material;
    loom_u8 transition_phase;
    loom_u8 presented_brightness;
    loom_u8 raster_enabled;
    loom_u8 adventure_last_request_kind;
    loom_u16 adventure_flags;
    loom_u16 adventure_request_count;
    loom_u8 adventure_last_request_id;
    loom_u8 adventure_pending_request_count;
    /* The player's animation set state and direction slot, or 0xff when the
     * scene has no set. */
    loom_u8 player_animation_state;
    loom_u8 player_animation_direction;
    /* The scene's actor pool: how many spawned, how many the world stopped
     * last tick, and where the first one stands. A ROM test watches a
     * behavior through these instead of compiler-private symbols. */
    loom_u8 actor_count;
    loom_u8 actor_blocked_count;
    loom_s16 actor_first_x;
    loom_s16 actor_first_y;
    /* The player's hit points, how many hits it has taken, how long it is
     * still ignoring them, and whether the run is over. */
    loom_u16 player_health;
    loom_u16 player_damage_count;
    loom_u8 player_invulnerable_ticks;
    loom_u8 game_over;
    /* Projectiles: how many spawns found the pool full, and how many landed
     * on something that could be hurt. An exhausted pool is an authoring
     * mistake, so it is a number a test can assert on rather than a silence. */
    loom_u16 actor_spawn_failures;
    loom_u16 projectile_hits;
    /* The first tile animation's frame and the first palette cycle's step
     * of the resident scene, so a ROM test can watch water move. */
    loom_u8 tile_animation_frame;
    loom_u8 palette_cycle_step;
    /* The first living controller actor (a second player), or 0, 0. */
    loom_s16 player_two_x;
    loom_s16 player_two_y;
    /* The surfaces' cells written and not yet sent, and the bytes their runs
     * carried in the last build: a board flushing under its budget
     * (SURF-001); missed_commit_count above says whether a tick overran. */
    loom_u16 surface_pending_cells;
    loom_u16 surface_last_bytes;
    /* The first two boards' cell hashes (GRID-001): whole states a ROM test
     * can pin after a replayed input sequence, one a pad. */
    loom_u16 board_hash;
    loom_u16 board_two_hash;
    /* Stompable actors killed from above (combat) and collectibles taken
     * (scene) since the last restart. */
    loom_u16 stomp_count;
    loom_u16 collect_count;
    /* The tick now running (logical_tick_count + 1), written as the tick
     * begins, right after the pads were sampled for it; logical_tick_count
     * above is written as it ends. A ROM test on the tick clock reads this
     * to know which tick will see the next frame's pad. */
    loom_u16 tick_started;
} LoomProjectDebugState;

LOOM_STATIC_ASSERT(loom_project_debug_state_is_one_hundred_eight_bytes,
                   sizeof(LoomProjectDebugState) == 108u);

#if defined(LOOM_BUILD_DEBUG)
extern volatile LoomProjectDebugState loom_project_debug_state;
extern volatile LoomUiTelemetry loom_project_ui_debug_state;
#endif

#endif
