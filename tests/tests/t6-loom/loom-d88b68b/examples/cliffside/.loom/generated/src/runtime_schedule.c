#include <loom/variables.h>
#include <loom/generated/variables.h>
#include <loom/generated/ui.h>
/* Generated direct top-down schedule; selected phases compile to ordinary calls. */
#include <loom/actor.h>
#include <loom/adventure.h>
#include <loom/animation.h>
#include <loom/audio.h>
#include <loom/camera.h>
#include <loom/combat.h>
#include <loom/frame.h>
#include <loom/game.h>
#include <loom/movement.h>
#include <loom/mode1.h>
#include <loom/project-debug.h>
#include <loom/save.h>
#include <loom/scene.h>
#include <loom/ui.h>
#include <loom/generated/runtime_schedule.h>
#include <loom/generated/user_hooks.h>

/* Exported project-generic readiness witness for debug and tests only. */
#if defined(LOOM_BUILD_DEBUG)
volatile loom_u16 loom_project_room_ready = 0xffffu;
#define LOOM_GENERATED_PUBLISH_ROOM_READY(value) \
    (loom_project_room_ready = (value))
#else
#define LOOM_GENERATED_PUBLISH_ROOM_READY(value) ((void)0)
#endif

#if defined(LOOM_BUILD_DEBUG)
volatile LoomProjectDebugState loom_project_debug_state;
volatile LoomUiTelemetry loom_project_ui_debug_state;
static loom_u16 loom_generated_debug_previous_scene;

static loom_u16 loom_generated_debug_increment(loom_u16 value)
{
return value == 0xffffu ? 0xffffu : (loom_u16)(value + 1u);
}

static loom_u16 loom_generated_debug_phase_mask(loom_u8 phase)
{
if (phase == LOOM_SCENE_TRANSITION_FADE_OUT) {
return LOOM_PROJECT_DEBUG_PHASE_FADE_OUT;
}
if (phase == LOOM_SCENE_TRANSITION_BLACK_WAIT) {
return LOOM_PROJECT_DEBUG_PHASE_BLACK_WAIT;
}
if (phase == LOOM_SCENE_TRANSITION_LOAD) {
return LOOM_PROJECT_DEBUG_PHASE_RESOURCE_SWAP;
}
if (phase == LOOM_SCENE_TRANSITION_FADE_IN) {
return LOOM_PROJECT_DEBUG_PHASE_FADE_IN;
}
if (phase == LOOM_SCENE_TRANSITION_FINAL_WAIT) {
return LOOM_PROJECT_DEBUG_PHASE_FINAL_WAIT;
}
return 0u;
}

static void loom_generated_debug_initialize(void)
{
loom_project_debug_state.magic[0] = (loom_u8)'L';
loom_project_debug_state.magic[1] = (loom_u8)'O';
loom_project_debug_state.magic[2] = (loom_u8)'O';
loom_project_debug_state.magic[3] = (loom_u8)'M';
loom_project_debug_state.schema_version =
LOOM_PROJECT_DEBUG_SCHEMA_VERSION;
loom_project_debug_state.logical_tick_count = 0u;
loom_project_debug_state.frame_id = 0u;
loom_project_debug_state.room_index = LOOM_SCENE_INVALID_INDEX;
loom_project_debug_state.room_ready = 0u;
loom_project_debug_state.room_enter_count[0] = 0u;
loom_project_debug_state.room_enter_count[1] = 0u;
loom_project_debug_state.player_x = 0;
loom_project_debug_state.player_y = 0;
loom_project_debug_state.camera_x = 0;
loom_project_debug_state.camera_y = 0;
loom_project_debug_state.blocked_movement_count = 0u;
loom_project_debug_state.transition_count = 0u;
loom_project_debug_state.transition_epoch = 0u;
loom_project_debug_state.transition_seen_mask = 0u;
loom_project_debug_state.black_frame_count = 0u;
loom_project_debug_state.trigger_dispatch_count = 0u;
loom_project_debug_state.current_music_cue = LOOM_INVALID_HANDLE;
loom_project_debug_state.last_audio_cue = LOOM_INVALID_HANDLE;
loom_project_debug_state.music_start_count = 0u;
loom_project_debug_state.sfx_dispatch_count = 0u;
loom_project_debug_state.missed_commit_count = 0u;
loom_project_debug_state.mixed_resource_presented_frame_count = 0u;
loom_project_debug_state.last_transition_source_scene =
LOOM_SCENE_INVALID_INDEX;
loom_project_debug_state.last_transition_target_scene =
LOOM_SCENE_INVALID_INDEX;
loom_project_debug_state.last_transition_spawn_x = 0;
loom_project_debug_state.last_transition_spawn_y = 0;
loom_project_debug_state.player_animation_frame =
LOOM_ANIMATION_INVALID_FRAME;
loom_project_debug_state.last_collision_material =
LOOM_MOVEMENT_COLLISION_NONE;
loom_project_debug_state.transition_phase =
LOOM_SCENE_TRANSITION_READY;
loom_project_debug_state.presented_brightness = 0u;
loom_project_debug_state.raster_enabled = LOOM_FALSE;
loom_project_debug_state.tile_animation_frame = 0u;
loom_project_debug_state.palette_cycle_step = 0u;
loom_project_debug_state.player_two_x = 0;
loom_project_debug_state.player_two_y = 0;
loom_project_debug_state.adventure_last_request_kind = 0u;
loom_project_debug_state.adventure_flags = 0u;
loom_project_debug_state.adventure_request_count = 0u;
loom_project_debug_state.adventure_last_request_id =
LOOM_ADVENTURE_REQUEST_NONE;
loom_project_debug_state.adventure_pending_request_count = 0u;
loom_project_debug_state.player_animation_state = 0u;
loom_project_debug_state.player_animation_direction = 0u;
loom_project_debug_state.actor_count = 0u;
loom_project_debug_state.actor_blocked_count = 0u;
loom_project_debug_state.actor_first_x = 0;
loom_project_debug_state.actor_first_y = 0;
loom_project_debug_state.player_health = 0u;
loom_project_debug_state.player_damage_count = 0u;
loom_project_debug_state.player_invulnerable_ticks = 0u;
loom_project_debug_state.game_over = LOOM_FALSE;
loom_project_debug_state.actor_spawn_failures = 0u;
loom_project_debug_state.projectile_hits = 0u;
loom_project_ui_debug_state.accepted_commits = 0u;
loom_project_ui_debug_state.repeated_boundaries = 0u;
loom_project_ui_debug_state.dialogue_page = LOOM_UI_SCREEN_NONE;
loom_project_ui_debug_state.last_commit_bytes = 0u;
loom_project_ui_debug_state.active_view = LOOM_UI_SCREEN_NONE;
loom_project_ui_debug_state.stack_depth = 0u;
loom_project_ui_debug_state.focus_index = 0u;
loom_project_ui_debug_state.commands_queued = 0u;
loom_project_ui_debug_state.last_commit_jobs = 0u;
loom_project_ui_debug_state.transition_pending = LOOM_FALSE;
loom_project_ui_debug_state.presented_map_slot = 0u;
loom_project_ui_debug_state.blocks_gameplay = LOOM_FALSE;
loom_project_ui_debug_state.reserved = 0u;
loom_generated_debug_previous_scene = LOOM_SCENE_INVALID_INDEX;
}

static LoomSceneDebugSnapshot loom_generated_scene_debug;
static loom_u8 loom_generated_scene_debug_seen;

static LoomMode1DebugSnapshot loom_generated_mode1_debug;
static loom_u8 loom_generated_mode1_debug_seen;

static LoomMovementDebugSnapshot loom_generated_movement_debug;
static loom_u8 loom_generated_movement_debug_seen;

static LoomAnimationDebugSnapshot loom_generated_animation_debug;

static LoomActorDebugSnapshot loom_generated_actor_debug;
static loom_u8 loom_generated_actor_debug_seen;

static LoomCombatDebugSnapshot loom_generated_combat_debug;
static loom_u8 loom_generated_combat_debug_seen;

static void loom_generated_debug_update(
    const LoomFrameBoundary *boundary,
    const LoomRuntimeCounters *counters)
{
    loom_u16 scene_index;
    loom_u8 transition_phase;

    loom_project_debug_state.frame_id = boundary->frame_id;
    loom_project_debug_state.logical_tick_count =
        counters->logical_tick_count == 0xffffu
            ? 0xffffu
            : (loom_u16)(counters->logical_tick_count + 1u);
    /* The inspector reads this struct as volatile; the runtime fills it field by field through a plain pointer. */
    (void)loom_ui_get_telemetry(
        (LoomUiTelemetry *)&loom_project_ui_debug_state);
    loom_project_debug_state.missed_commit_count =
        counters->missed_frame_count;
    if (loom_scene_debug_epoch != loom_generated_scene_debug_seen) {
        loom_generated_scene_debug_seen = loom_scene_debug_epoch;
        loom_scene_debug_snapshot(&loom_generated_scene_debug);
        loom_project_debug_state.transition_count =
            loom_generated_scene_debug.transition_count;
        loom_project_debug_state.black_frame_count =
            loom_generated_scene_debug.black_frame_count;
        loom_project_debug_state.trigger_dispatch_count =
            loom_generated_scene_debug.trigger_dispatch_count;
        loom_project_debug_state.last_transition_source_scene =
            loom_generated_scene_debug.last_transition_source;
        loom_project_debug_state.last_transition_target_scene =
            loom_generated_scene_debug.last_transition_target;
        loom_project_debug_state.last_transition_spawn_x =
            loom_generated_scene_debug.last_transition_spawn_x;
        loom_project_debug_state.last_transition_spawn_y =
            loom_generated_scene_debug.last_transition_spawn_y;
    }
    scene_index = loom_generated_scene_debug.scene_index;
    transition_phase = loom_generated_scene_debug.transition_phase;
    if (transition_phase != LOOM_SCENE_TRANSITION_READY) {
        loom_u16 epoch;

        epoch = loom_project_debug_state.transition_count == 0xffffu
            ? 0xffffu
            : (loom_u16)(loom_project_debug_state.transition_count + 1u);
        if (loom_project_debug_state.transition_epoch != epoch) {
            loom_project_debug_state.transition_epoch = epoch;
            loom_project_debug_state.transition_seen_mask = 0u;
        }
        loom_project_debug_state.transition_seen_mask |=
            loom_generated_debug_phase_mask(transition_phase);
    } else if (loom_project_debug_state.transition_count != 0u) {
        loom_project_debug_state.transition_epoch =
            loom_project_debug_state.transition_count;
    }
    if (scene_index != loom_generated_debug_previous_scene) {
        if (scene_index == 0u) {
            loom_project_debug_state.room_enter_count[0] =
                loom_generated_debug_increment(
                    loom_project_debug_state.room_enter_count[0]);
        } else if (scene_index == 1u) {
            loom_project_debug_state.room_enter_count[1] =
                loom_generated_debug_increment(
                    loom_project_debug_state.room_enter_count[1]);
        }
        loom_generated_debug_previous_scene = scene_index;
    }
    loom_project_debug_state.room_index = scene_index;
    loom_project_debug_state.transition_phase = transition_phase;
    if (loom_mode1_debug_epoch != loom_generated_mode1_debug_seen) {
        loom_generated_mode1_debug_seen = loom_mode1_debug_epoch;
        loom_mode1_debug_snapshot(&loom_generated_mode1_debug);
        loom_project_debug_state.room_ready =
            loom_generated_mode1_debug.ready != LOOM_FALSE ? 1u : 0u;
        loom_project_debug_state.camera_x = loom_generated_mode1_debug.camera_x;
        loom_project_debug_state.camera_y = loom_generated_mode1_debug.camera_y;
        loom_project_debug_state.presented_brightness =
            loom_generated_mode1_debug.brightness;
        loom_project_debug_state.raster_enabled =
            loom_generated_mode1_debug.raster_enabled;
        loom_project_debug_state.tile_animation_frame =
            loom_generated_mode1_debug.tile_animation_frame;
        loom_project_debug_state.palette_cycle_step =
            loom_generated_mode1_debug.palette_cycle_step;
    }
    if ((transition_phase == LOOM_SCENE_TRANSITION_LOAD &&
         loom_generated_mode1_debug.brightness != 0u) ||
        (transition_phase == LOOM_SCENE_TRANSITION_FADE_IN &&
         loom_generated_mode1_debug.ready == LOOM_FALSE) ||
        (transition_phase == LOOM_SCENE_TRANSITION_BLACK_WAIT &&
         loom_generated_mode1_debug.brightness != 0u)) {
        loom_project_debug_state.mixed_resource_presented_frame_count =
            loom_generated_debug_increment(
                loom_project_debug_state
                    .mixed_resource_presented_frame_count);
    }
    if (loom_movement_debug_epoch != loom_generated_movement_debug_seen) {
        loom_generated_movement_debug_seen = loom_movement_debug_epoch;
        loom_movement_debug_snapshot(&loom_generated_movement_debug);
        loom_project_debug_state.player_x = loom_generated_movement_debug.player_x;
        loom_project_debug_state.player_y = loom_generated_movement_debug.player_y;
        loom_project_debug_state.blocked_movement_count =
            loom_generated_movement_debug.blocked_count;
        loom_project_debug_state.last_collision_material =
            loom_generated_movement_debug.last_collision;
    }
    loom_animation_debug_snapshot(
        loom_generated_scene_debug.player_slot,
        &loom_generated_animation_debug);
    loom_project_debug_state.player_animation_frame =
        loom_generated_animation_debug.frame_index;
    loom_project_debug_state.player_animation_state =
        loom_generated_animation_debug.state;
    loom_project_debug_state.player_animation_direction =
        loom_generated_animation_debug.direction;
    if (loom_actor_debug_epoch != loom_generated_actor_debug_seen) {
        loom_generated_actor_debug_seen = loom_actor_debug_epoch;
        loom_actor_debug_snapshot(&loom_generated_actor_debug);
        loom_project_debug_state.actor_count = loom_generated_actor_debug.count;
        loom_project_debug_state.actor_blocked_count =
            loom_generated_actor_debug.blocked_count;
        loom_project_debug_state.actor_first_x =
            loom_generated_actor_debug.first_x;
        loom_project_debug_state.actor_first_y =
            loom_generated_actor_debug.first_y;
        loom_project_debug_state.actor_spawn_failures =
            loom_actor_spawn_failures();
        {
            loom_s16 two_x;
            loom_s16 two_y;

            if (loom_actor_controller_position(&two_x, &two_y) != LOOM_FALSE) {
                loom_project_debug_state.player_two_x = two_x;
                loom_project_debug_state.player_two_y = two_y;
            } else {
                loom_project_debug_state.player_two_x = 0;
                loom_project_debug_state.player_two_y = 0;
            }
        }
    }
    if (loom_combat_debug_epoch != loom_generated_combat_debug_seen) {
        loom_generated_combat_debug_seen = loom_combat_debug_epoch;
        loom_combat_debug_snapshot(&loom_generated_combat_debug);
        loom_project_debug_state.player_health =
            loom_generated_combat_debug.player_health;
        loom_project_debug_state.player_damage_count =
            loom_generated_combat_debug.damage_count;
        loom_project_debug_state.player_invulnerable_ticks =
            loom_generated_combat_debug.invulnerable_ticks;
        loom_project_debug_state.game_over =
            loom_generated_combat_debug.game_over;
        loom_project_debug_state.projectile_hits =
            loom_generated_combat_debug.projectile_hits;
    }
}
#endif

static loom_u8 loom_generated_gameplay_blocked;

static loom_u16 loom_generated_ui_published_health = 0xffffu;

static loom_u8 loom_generated_game_over_shown;
static loom_u16 loom_generated_published_health = 0xffffu;

const loom_u8 loom_generated_save_enabled = LOOM_FALSE;
const loom_u16 loom_generated_save_layout = 0x79f6u;
const loom_u16 loom_generated_save_sram_bytes = 0u;

static const loom_u8 loom_generated_scene_pair_clamp[1] = { LOOM_TRUE };

static void loom_generated_ui_actions(void)
{
    LoomUiCommandHandle command;
    loom_u8 seen;

    for (seen = 0u; seen < LOOM_UI_COMMAND_QUEUE_CAPACITY; ++seen) {
        if (loom_ui_poll_command(&command) != LOOM_STATUS_OK) {
            break;
        }
        if (command == (LoomUiCommandHandle)12547u) {
            (void)loom_variable_initialize();
            (void)loom_combat_initialize();
            loom_generated_game_over_shown = LOOM_FALSE;
            (void)loom_scene_initialize();
            (void)loom_ui_replace_view((LoomUiViewHandle)50644u);
        } else if (command == (LoomUiCommandHandle)12950u) {
            (void)loom_ui_replace_view((LoomUiViewHandle)52065u);
        } else {
            (void)loom_ui_enqueue_command(command);
        }
    }
}

loom_u16 loom_generated_adventure_visibility_epoch(void)
{
    return 0u;
}

loom_u8 loom_generated_adventure_gate_allows(
    const LoomSceneTrigger *trigger)
{
    (void)trigger;
    return LOOM_TRUE;
}

LoomStatus loom_generated_adventure_apply_visibility(
    const LoomSceneTrigger *trigger)
{
    (void)trigger;
    return LOOM_STATUS_OK;
}

LoomStatus loom_generated_adventure_try_action(
    const LoomSceneTrigger *trigger,
    const LoomInputSnapshot *input,
    loom_u8 entering,
    loom_u8 *dispatched)
{
    (void)trigger;
    (void)input;
    (void)entering;
    *dispatched = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

loom_u8 loom_generated_actor_blocks_box(loom_s16 left,
    loom_s16 top,
    loom_s16 right,
    loom_s16 bottom)
{
    return loom_actor_blocks_box(left, top, right, bottom);
}

loom_u8 loom_generated_actor_floor_below(loom_s16 left,
    loom_s16 right,
    loom_s16 top,
    loom_s16 bottom,
    loom_s16 *floor)
{
    return loom_actor_floor_below(left, right, top, bottom, LOOM_ACTOR_INVALID_INDEX, floor);
}

LoomStatus loom_generated_actor_drive_animation(loom_u8 slot,
    loom_u8 moving,
    loom_s8 facing_x,
    loom_s8 facing_y,
    loom_u8 air)
{
    return loom_animation_drive_slot(slot, moving, facing_x, facing_y, air);
}

static LoomStatus loom_generated_phase_read_actions(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    (void)boundary;
    (void)input;
    (void)counters;
    (void)loom_variable_reset_lifetime(LOOM_VARIABLE_LIFETIME_FRAME);
    if (loom_scene_transition_phase() == LOOM_SCENE_TRANSITION_LOAD) {
        (void)loom_variable_reset_lifetime(LOOM_VARIABLE_LIFETIME_SCENE);
    }
    {
        loom_u16 published;

        published = LOOM_VAR_HEALTH_WORD;
        if (published != loom_generated_ui_published_health) {
            loom_generated_ui_published_health = published;
            (void)loom_ui_set_u16(LOOM_UI_BINDING_HEALTH, published);
        }
    }
    loom_generated_gameplay_blocked = loom_ui_blocks_gameplay();
    return loom_ui_update(input, loom_scene_index(), 0u);
}

static LoomStatus loom_generated_phase_update_behaviors(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    (void)boundary;
    (void)input;
    (void)counters;
    loom_game_advance_tick();
    loom_generated_ui_actions();
    if (loom_generated_game_over_shown == LOOM_FALSE &&
        loom_combat_game_over() != LOOM_FALSE) {
        loom_generated_game_over_shown = LOOM_TRUE;
        (void)loom_ui_replace_view((LoomUiViewHandle)50763u);
    }
    if (loom_generated_gameplay_blocked != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_generated_phase_move_and_collide(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    LoomStatus status;

    (void)boundary;
    (void)input;
    (void)counters;
    if (loom_generated_gameplay_blocked != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    {
        loom_s16 pair_x;
        loom_s16 pair_y;
        loom_u8 framed;

        framed = (loom_u8)(loom_scene_index() < 1u &&
                           loom_generated_scene_pair_clamp[loom_scene_index()] != LOOM_FALSE &&
                           loom_actor_controller_position(&pair_x, &pair_y) != LOOM_FALSE);
        loom_movement_set_view_box(
            (loom_s16)(loom_camera_x() + 8), (loom_s16)(loom_camera_y() + 8),
            (loom_s16)(loom_camera_x() + 247), (loom_s16)(loom_camera_y() + 215), framed);
        loom_actor_set_view_box(
            (loom_s16)(loom_camera_x() + 8), (loom_s16)(loom_camera_y() + 8),
            (loom_s16)(loom_camera_x() + 247), (loom_s16)(loom_camera_y() + 215), framed);
    }
    status = loom_scene_move(input);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    status = loom_combat_update();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    {
        loom_u16 health;

        health = loom_combat_player_health();
        if (health != loom_generated_published_health) {
            loom_generated_published_health = health;
            (void)loom_variable_set(LOOM_VAR_HEALTH, health);
        }
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_generated_phase_dispatch_triggers(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    (void)boundary;
    (void)input;
    (void)counters;
    if (loom_generated_gameplay_blocked != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    return loom_scene_dispatch_triggers(boundary, input);
}

static LoomStatus loom_generated_phase_advance_animation(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    LoomStatus status;

    (void)boundary;
    (void)input;
    (void)counters;
    if (loom_generated_gameplay_blocked != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    /* Movement-driven sets pick their state before the clip advances. */
    status = loom_animation_drive(loom_movement_facing_x(),
                                 loom_movement_facing_y(),
                                 loom_movement_moving(),
                                 loom_movement_air());
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    return loom_scene_advance_animation();
}

static LoomStatus loom_generated_phase_build_render(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    LoomStatus status;

    (void)boundary;
    (void)input;
    (void)counters;
    loom_camera_set_facing(loom_movement_facing_x(), loom_movement_facing_y());
    {
        loom_s16 second_x;
        loom_s16 second_y;
        loom_u8 present;

        present = loom_actor_controller_position(&second_x, &second_y);
        loom_camera_set_second_target(present != LOOM_FALSE ? second_x : 0, present != LOOM_FALSE ? second_y : 0, present);
    }
    status = loom_scene_update_camera();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    return loom_mode1_build_frame(boundary);
}

static LoomStatus loom_generated_phase_submit_frame(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    (void)boundary;
    (void)input;
    (void)counters;
    return loom_frame_build_submit();
}

LoomStatus loom_generated_runtime_initialize(void)
{
    LoomStatus status;

    LOOM_RUNTIME_DEBUG_INIT_STEP(10);
    status = loom_frame_build_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(11);
    status = loom_mode1_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(12);
    status = loom_movement_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(13);
    status = loom_animation_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(14);
    status = loom_camera_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(15);
    status = loom_variable_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(16);
    status = loom_actor_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(17);
    status = loom_combat_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_generated_game_over_shown = LOOM_FALSE;
    LOOM_RUNTIME_DEBUG_INIT_STEP(18);
    status = loom_scene_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(19);
    status = loom_ui_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_generated_gameplay_blocked = LOOM_FALSE;
    LOOM_GENERATED_PUBLISH_ROOM_READY(0u);
#if defined(LOOM_BUILD_DEBUG)
    loom_generated_debug_initialize();
#endif
    return LOOM_STATUS_OK;
}

LoomStatus loom_generated_runtime_tick(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters)
{
    LoomStatus status;

    if (boundary == (const LoomFrameBoundary *)0 ||
        input == (const LoomInputSnapshot *)0 ||
        counters == (const LoomRuntimeCounters *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_frame_build_begin();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_READ_ACTIONS);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_READ_ACTIONS);
    status = loom_generated_phase_read_actions(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_UPDATE_BEHAVIORS);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_UPDATE_BEHAVIORS);
    status = loom_generated_phase_update_behaviors(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_MOVE_AND_COLLIDE);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_MOVE_AND_COLLIDE);
    status = loom_generated_phase_move_and_collide(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_DISPATCH_TRIGGERS);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_DISPATCH_TRIGGERS);
    status = loom_generated_phase_dispatch_triggers(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_ADVANCE_ANIMATION);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_ADVANCE_ANIMATION);
    status = loom_generated_phase_advance_animation(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_BUILD_RENDER);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_BUILD_RENDER);
    status = loom_generated_phase_build_render(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_SUBMIT_FRAME);
    LOOM_RUNTIME_DEBUG_INIT_STEP(100u + LOOM_RUNTIME_PHASE_SUBMIT_FRAME);
    status = loom_generated_phase_submit_frame(boundary, input, counters);
    if (status != LOOM_STATUS_OK) {
        loom_frame_build_abort();
        return status;
    }
    LOOM_GENERATED_PUBLISH_ROOM_READY(
        loom_mode1_ready() != LOOM_FALSE ? 1u : 0u);
#if defined(LOOM_BUILD_DEBUG)
    LOOM_RUNTIME_TRACE_PHASE(LOOM_RUNTIME_PHASE_DEBUG_WITNESS);
    loom_generated_debug_update(boundary, counters);
#endif
    return LOOM_STATUS_OK;
}
