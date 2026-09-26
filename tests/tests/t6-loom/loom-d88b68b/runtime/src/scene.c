#include <loom/scene.h>

#if LOOM_SURFACES_ENABLED
#include <loom/surface.h>
#endif
#if LOOM_BOARDS_ENABLED
#include <loom/board.h>
#endif

typedef struct LoomSceneState {
    LoomCommitId black_commit_id;
    LoomCommitId final_commit_id;
    loom_u16 scene_index;
    /* The active record; the table walk in loom_scene_record_at costs a
     * scanline per call on 816-tcc and the tick needed it twice. */
    const LoomSceneRecord *record;
    loom_u16 target_scene;
    loom_s16 target_spawn_x;
    loom_s16 target_spawn_y;
    loom_u16 inside_mask;
    loom_u16 once_mask;
    /* Collectibles taken since this activation; cleared with once_mask. */
    loom_u16 taken_mask;
    loom_u16 visibility_epoch;
    loom_u8 visibility_pending;
    /* Where the player stood when the last full check found it inside no
     * live trigger; the same place and the same flags find nothing again. */
    loom_u8 idle_valid;
    loom_s16 idle_x;
    loom_s16 idle_y;
    loom_u16 transition_count;
    loom_u16 trigger_dispatch_count;
    loom_u16 collect_count;
    loom_u16 black_frame_count;
    loom_u16 last_transition_source;
    loom_u16 last_transition_target;
    loom_s16 last_transition_spawn_x;
    loom_s16 last_transition_spawn_y;
    loom_u8 transition_phase;
    loom_u8 initialized;
} LoomSceneState;

static LoomSceneState loom_scene_state;

/* Generated direct dispatch; no function-pointer table is retained on target. */
LoomStatus loom_generated_dispatch_user_hook(loom_u16 hook_id);
loom_u8 loom_generated_adventure_gate_allows(
    const LoomSceneTrigger *trigger);
LoomStatus loom_generated_adventure_apply_visibility(
    const LoomSceneTrigger *trigger);
/* Changes whenever gated visibility may have changed (the adventure flags). */
loom_u16 loom_generated_adventure_visibility_epoch(void);
LoomStatus loom_generated_adventure_try_action(
    const LoomSceneTrigger *trigger,
    const LoomInputSnapshot *input,
    loom_u8 entering,
    loom_u8 *dispatched);

static const LoomSceneRecord *loom_scene_record_at(loom_u16 scene_index)
{
    const LoomSceneRecord *scene;

    /* 816-tcc lowers variable struct-array subscripts through its generic
     * multiplication helper. Walk this fixed, tiny table so scene dispatch
     * does not depend on scratch state outside Loom's target-port contract. */
    scene = loom_generated_runtime_scenes;
    while (scene_index != 0u) {
        ++scene;
        --scene_index;
    }
    return scene;
}

static void loom_scene_increment(loom_u16 *value)
{
    if (*value != 0xffffu) {
        ++*value;
    }
}

static loom_u8 loom_scene_commit_was_presented(
    const LoomFrameBoundary *boundary,
    LoomCommitId commit_id)
{
    return (loom_u8)(commit_id != LOOM_COMMIT_NONE &&
                     boundary->presentation == LOOM_PRESENTATION_NEW_COMMIT &&
                     boundary->presented_commit_id == commit_id);
}

static LoomStatus loom_scene_record_build_commit(LoomCommitId *commit_id)
{
    return loom_frame_build_current_commit_id(commit_id);
}

loom_u8 loom_scene_debug_epoch;

static LoomStatus loom_scene_activate(loom_u16 scene_index,
                                      loom_s16 spawn_x,
                                      loom_s16 spawn_y)
{
    const LoomSceneRecord *scene;
    LoomStatus status;

    if (scene_index >= loom_generated_scene_count) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    scene = loom_scene_record_at(scene_index);
    /* A room whose profile never moves anything -- a board, a title card --
     * carries no movement scene, and activates without one. */
    if (scene->mode1 == (const LoomMode1Scene *)0 ||
        scene->trigger_count > LOOM_SCENE_TRIGGER_CAPACITY ||
        (scene->trigger_count != 0u &&
         scene->triggers == (const LoomSceneTrigger *)0) ||
        (scene->hook_count != 0u &&
         scene->hook_ids == (const loom_u16 *)0)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_mode1_activate_scene(scene->mode1);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#if LOOM_SURFACES_ENABLED
    status = loom_surface_activate_scene(scene->surfaces);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif
#if LOOM_BOARDS_ENABLED
    status = loom_board_activate_scene(scene->boards);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif
    if (loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        status = loom_mode1_set_raster_enabled(LOOM_FALSE);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    if (scene->movement != (const LoomMovementScene *)0) {
        status = loom_movement_activate_scene(scene->movement, spawn_x, spawn_y);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    status = loom_animation_activate_scene(scene->animation);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    status = loom_camera_activate_scene(scene->camera);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    /* A room exit carries a joined second player through; a fresh start
     * (the initial scene, a restart) does not. */
    if (loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        loom_actor_note_second_player();
    }
    status = loom_actor_activate_scene(scene->actors);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if (loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        loom_actor_restore_second_player(
            (loom_s16)(loom_movement_player_x() - 16), loom_movement_player_y());
    }
    loom_scene_state.scene_index = scene_index;
    loom_scene_state.record = scene;
    ++loom_scene_debug_epoch;
    loom_scene_state.inside_mask = 0u;
    loom_scene_state.visibility_pending = LOOM_TRUE;
    loom_scene_state.idle_valid = LOOM_FALSE;
    loom_scene_state.once_mask = 0u;
    loom_scene_state.taken_mask = 0u;
    return LOOM_STATUS_OK;
}

/* The player's collider box for this tick, computed once per trigger scan.
 * Scene coordinates stay within +-16384 pixels and boxes within 4096, so
 * every edge fits a signed 16-bit value; 32-bit math costs library calls on
 * the 65816. */
typedef struct LoomSceneBox {
    loom_s16 left;
    loom_s16 top;
    loom_s16 right;
    loom_s16 bottom;
} LoomSceneBox;

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* runtime/backends/pvsneslib/src/scene.asm; the C rendition follows. */
#define LOOM_SCENE_INTERSECT_FAST 1
loom_u16 loom_pvs_scene_intersect_mask(const LoomSceneBox *player,
                                       const LoomSceneTrigger *triggers,
                                       loom_u8 count);
#else
/* Bit i is set when trigger i overlaps the player box. */
static loom_u16 loom_scene_intersect_mask(const LoomSceneBox *player,
                                          const LoomSceneTrigger *triggers,
                                          loom_u8 count)
{
    loom_u16 mask;
    loom_u16 bit;

    mask = 0u;
    for (bit = 1u; count != 0u; --count, ++triggers, bit = (loom_u16)(bit << 1)) {
        if (player->left <
                (loom_s16)(triggers->x + (loom_s16)triggers->width) &&
            player->right > triggers->x &&
            player->top <
                (loom_s16)(triggers->y + (loom_s16)triggers->height) &&
            player->bottom > triggers->y) {
            mask |= bit;
        }
    }
    return mask;
}
#endif

static void loom_scene_player_box(LoomSceneBox *box)
{
    const LoomMovementScene *movement;

    movement = loom_scene_state.record->movement;
    box->left = (loom_s16)(loom_movement_player_x() + movement->collider_x);
    box->top = (loom_s16)(loom_movement_player_y() + movement->collider_y);
    box->right = (loom_s16)(box->left + (loom_s16)movement->collider_width);
    box->bottom = (loom_s16)(box->top + (loom_s16)movement->collider_height);
}

static LoomStatus loom_scene_dispatch_hook_range(
    const LoomSceneRecord *scene,
    const LoomSceneTrigger *trigger)
{
    loom_u16 index;

    if (trigger->first_hook > scene->hook_count ||
        trigger->hook_count >
            (loom_u16)(scene->hook_count - trigger->first_hook)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < trigger->hook_count; ++index) {
        LoomStatus status;

        status = loom_generated_dispatch_user_hook(
            scene->hook_ids[trigger->first_hook + index]);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        loom_scene_increment(&loom_scene_state.trigger_dispatch_count);
        ++loom_scene_debug_epoch;
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_scene_start_transition(
    const LoomSceneTrigger *trigger)
{
    LoomStatus status;

    if (trigger->target_scene == LOOM_SCENE_INVALID_INDEX ||
        trigger->target_scene >= loom_generated_scene_count) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    status = loom_mode1_set_raster_enabled(LOOM_FALSE);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_scene_state.target_scene = trigger->target_scene;
    loom_scene_state.target_spawn_x = trigger->target_spawn_x;
    loom_scene_state.target_spawn_y = trigger->target_spawn_y;
    loom_scene_state.last_transition_source = loom_scene_state.scene_index;
    loom_scene_state.last_transition_target = trigger->target_scene;
    loom_scene_state.last_transition_spawn_x = trigger->target_spawn_x;
    loom_scene_state.last_transition_spawn_y = trigger->target_spawn_y;
    loom_scene_state.black_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.final_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.transition_phase = LOOM_SCENE_TRANSITION_FADE_OUT;
    ++loom_scene_debug_epoch;
    return LOOM_STATUS_OK;
}

static LoomStatus loom_scene_check_triggers(const LoomInputSnapshot *input)
{
    const LoomSceneRecord *scene;
    const LoomSceneTrigger *trigger;
    loom_u16 inside_mask;
    loom_u16 index;
    loom_u16 bit;
    loom_u16 hits;
    loom_u16 epoch;
    loom_u8 apply_visibility;
    LoomSceneBox player;

    scene = loom_scene_state.record;
    trigger = scene->triggers;
    inside_mask = 0u;
    /* Gated sprites change only when the flags do, so their visibility is
     * re-applied on activation and on flag changes, not every tick. */
    epoch = loom_generated_adventure_visibility_epoch();
    {
        loom_s16 player_x;
        loom_s16 player_y;

        player_x = loom_movement_player_x();
        player_y = loom_movement_player_y();
        /* The idle tick: the player where it stood last tick, inside no live
         * trigger, with the flags (and so every gate) unchanged and the room
         * not re-entered (which sets visibility_pending): the check below
         * would find nothing, so it is skipped. */
        if (loom_scene_state.idle_valid != LOOM_FALSE &&
            loom_scene_state.visibility_pending == LOOM_FALSE &&
            epoch == loom_scene_state.visibility_epoch &&
            player_x == loom_scene_state.idle_x &&
            player_y == loom_scene_state.idle_y) {
            return LOOM_STATUS_OK;
        }
        loom_scene_state.idle_valid = LOOM_FALSE;
        loom_scene_state.idle_x = player_x;
        loom_scene_state.idle_y = player_y;
    }
    loom_scene_player_box(&player);
    apply_visibility =
        (loom_u8)(loom_scene_state.visibility_pending != LOOM_FALSE ||
                  epoch != loom_scene_state.visibility_epoch);
    loom_scene_state.visibility_epoch = epoch;
    loom_scene_state.visibility_pending = LOOM_FALSE;
    if (apply_visibility != LOOM_FALSE) {
        /* The first tick after activation and every flag change: check the
         * records and re-apply gated visibility. */
        const LoomSceneTrigger *check;

        check = trigger;
        for (index = 0u; index < scene->trigger_count; ++index, ++check) {
            LoomStatus status;

            if (check->width == 0u || check->height == 0u ||
                (check->flags &
                 (loom_u8)(~(LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY |
                             LOOM_SCENE_TRIGGER_EXIT))) != 0u) {
                return LOOM_STATUS_INVALID_ARGUMENT;
            }
            status = loom_generated_adventure_apply_visibility(check);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            /* A taken collectible stays hidden through a flag change. */
            if ((loom_scene_state.taken_mask & (loom_u16)(1u << index)) != 0u &&
                check->sprite_slot != 0xffu) {
                status = loom_mode1_set_sprite_visible(check->sprite_slot,
                                                       LOOM_FALSE);
                if (status != LOOM_STATUS_OK) {
                    return status;
                }
            }
        }
    }
    /* The box test for every trigger at once; most ticks hit nothing, and
     * the gate check that walks the adventure kit's flag word runs only for
     * the triggers under the player. */
#if defined(LOOM_SCENE_INTERSECT_FAST)
    hits = loom_pvs_scene_intersect_mask(&player, trigger,
                                         scene->trigger_count);
#else
    hits = loom_scene_intersect_mask(&player, trigger, scene->trigger_count);
#endif
    bit = 1u;
    for (index = 0u; hits != 0u;
         ++index, ++trigger, bit = (loom_u16)(bit << 1),
         hits = (loom_u16)(hits >> 1)) {
        loom_u8 entering;
        loom_u8 adventure_dispatched;
        LoomStatus status;

        if ((hits & 1u) == 0u) {
            continue;
        }
        if ((loom_scene_state.taken_mask & bit) != 0u ||
            loom_generated_adventure_gate_allows(trigger) == LOOM_FALSE) {
            continue;
        }
        inside_mask |= bit;
        entering = (loom_u8)((loom_scene_state.inside_mask & bit) == 0u);
        if ((trigger->flags & LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY) != 0u &&
            entering == LOOM_FALSE) {
            continue;
        }
        status = loom_generated_adventure_try_action(
            trigger, input, entering, &adventure_dispatched);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        if (trigger->adventure_action != LOOM_ADVENTURE_ACTION_NONE &&
            adventure_dispatched == LOOM_FALSE) {
            continue;
        }
        if (trigger->adventure_action == LOOM_ADVENTURE_ACTION_COLLECT) {
            /* Taken: hidden and silent until the room is entered again. */
            loom_scene_state.taken_mask |= bit;
            loom_scene_increment(&loom_scene_state.collect_count);
            ++loom_scene_debug_epoch;
            if (trigger->sprite_slot != 0xffu) {
                status = loom_mode1_set_sprite_visible(trigger->sprite_slot,
                                                       LOOM_FALSE);
                if (status != LOOM_STATUS_OK) {
                    return status;
                }
            }
        }
        status = loom_scene_dispatch_hook_range(scene, trigger);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        if ((trigger->flags & LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY) != 0u) {
            loom_scene_state.once_mask |= bit;
        }
        if ((trigger->flags & LOOM_SCENE_TRIGGER_EXIT) != 0u) {
            loom_scene_state.inside_mask = inside_mask;
            return loom_scene_start_transition(trigger);
        }
    }
    loom_scene_state.inside_mask = inside_mask;
    loom_scene_state.once_mask &= inside_mask;
    loom_scene_state.idle_valid = (loom_u8)(inside_mask == 0u);
    return LOOM_STATUS_OK;
}

static LoomStatus loom_scene_update_transition(
    const LoomFrameBoundary *boundary)
{
    LoomStatus status;
    loom_u8 brightness;

    if (loom_scene_state.transition_phase ==
        LOOM_SCENE_TRANSITION_FADE_OUT) {
        brightness = loom_mode1_brightness();
        if (brightness != 0u) {
            --brightness;
            status = loom_mode1_set_brightness(brightness);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        if (brightness == 0u) {
            status = loom_scene_record_build_commit(
                &loom_scene_state.black_commit_id);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            loom_scene_state.transition_phase =
                LOOM_SCENE_TRANSITION_BLACK_WAIT;
        }
        return LOOM_STATUS_OK;
    }
    if (loom_scene_state.transition_phase ==
        LOOM_SCENE_TRANSITION_BLACK_WAIT) {
        if (loom_scene_commit_was_presented(
                boundary, loom_scene_state.black_commit_id) != LOOM_FALSE) {
            loom_scene_increment(&loom_scene_state.black_frame_count);
            status = loom_scene_activate(loom_scene_state.target_scene,
                                         loom_scene_state.target_spawn_x,
                                         loom_scene_state.target_spawn_y);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            status = loom_mode1_set_brightness(0u);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            loom_scene_state.transition_phase =
                LOOM_SCENE_TRANSITION_LOAD;
            return LOOM_STATUS_OK;
        }
        return loom_scene_record_build_commit(
            &loom_scene_state.black_commit_id);
    }
    if (loom_scene_state.transition_phase == LOOM_SCENE_TRANSITION_LOAD) {
        if (loom_mode1_ready() != LOOM_FALSE) {
            status = loom_mode1_set_raster_enabled(LOOM_TRUE);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            loom_scene_state.transition_phase =
                LOOM_SCENE_TRANSITION_FADE_IN;
        }
        return LOOM_STATUS_OK;
    }
    if (loom_scene_state.transition_phase ==
        LOOM_SCENE_TRANSITION_FADE_IN) {
        brightness = loom_mode1_brightness();
        if (brightness < 15u) {
            ++brightness;
            status = loom_mode1_set_brightness(brightness);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        if (brightness == 15u) {
            status = loom_scene_record_build_commit(
                &loom_scene_state.final_commit_id);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            loom_scene_state.transition_phase =
                LOOM_SCENE_TRANSITION_FINAL_WAIT;
        }
        return LOOM_STATUS_OK;
    }
    if (loom_scene_state.transition_phase ==
        LOOM_SCENE_TRANSITION_FINAL_WAIT) {
        if (loom_scene_commit_was_presented(
                boundary, loom_scene_state.final_commit_id) != LOOM_FALSE) {
            loom_scene_state.transition_phase =
                LOOM_SCENE_TRANSITION_READY;
            loom_scene_state.target_scene = LOOM_SCENE_INVALID_INDEX;
            loom_scene_increment(&loom_scene_state.transition_count);
            return LOOM_STATUS_OK;
        }
        return loom_scene_record_build_commit(
            &loom_scene_state.final_commit_id);
    }
    return LOOM_STATUS_INVALID_ARGUMENT;
}

LoomStatus loom_scene_initialize(void)
{
    LoomStatus status;

    loom_scene_state.black_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.final_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.scene_index = LOOM_SCENE_INVALID_INDEX;
    loom_scene_state.record = (const LoomSceneRecord *)0;
    ++loom_scene_debug_epoch;
    loom_scene_state.target_scene = LOOM_SCENE_INVALID_INDEX;
    loom_scene_state.target_spawn_x = 0;
    loom_scene_state.target_spawn_y = 0;
    loom_scene_state.inside_mask = 0u;
    loom_scene_state.visibility_pending = LOOM_TRUE;
    loom_scene_state.idle_valid = LOOM_FALSE;
    loom_scene_state.once_mask = 0u;
    loom_scene_state.taken_mask = 0u;
    loom_scene_state.transition_count = 0u;
    loom_scene_state.trigger_dispatch_count = 0u;
    loom_scene_state.collect_count = 0u;
    loom_scene_state.black_frame_count = 0u;
    loom_scene_state.last_transition_source = LOOM_SCENE_INVALID_INDEX;
    loom_scene_state.last_transition_target = LOOM_SCENE_INVALID_INDEX;
    loom_scene_state.last_transition_spawn_x = 0;
    loom_scene_state.last_transition_spawn_y = 0;
    loom_scene_state.transition_phase = LOOM_SCENE_TRANSITION_READY;
    loom_scene_state.initialized = LOOM_TRUE;
    if (loom_generated_scene_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    if (loom_generated_scene_count == 0u ||
        loom_generated_scene_initial_index >= loom_generated_scene_count) {
        loom_scene_state.initialized = LOOM_FALSE;
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_scene_activate(loom_generated_scene_initial_index,
                                 loom_generated_scene_initial_spawn_x,
                                 loom_generated_scene_initial_spawn_y);
    if (status != LOOM_STATUS_OK) {
        loom_scene_state.initialized = LOOM_FALSE;
    }
    return status;
}

/* A save restores here: the same reset initialise does, then the saved
 * scene at the saved spawn, activated at once rather than through a
 * transition. */
LoomStatus loom_scene_load(loom_u16 scene_index,
                           loom_s16 spawn_x,
                           loom_s16 spawn_y)
{
    LoomStatus status;

    if (loom_scene_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_scene_enabled == LOOM_FALSE ||
        scene_index >= loom_generated_scene_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_scene_state.black_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.final_commit_id = LOOM_COMMIT_NONE;
    loom_scene_state.target_scene = LOOM_SCENE_INVALID_INDEX;
    loom_scene_state.target_spawn_x = 0;
    loom_scene_state.target_spawn_y = 0;
    loom_scene_state.inside_mask = 0u;
    loom_scene_state.visibility_pending = LOOM_TRUE;
    loom_scene_state.idle_valid = LOOM_FALSE;
    loom_scene_state.once_mask = 0u;
    loom_scene_state.taken_mask = 0u;
    loom_scene_state.transition_phase = LOOM_SCENE_TRANSITION_READY;
    ++loom_scene_debug_epoch;
    status = loom_scene_activate(scene_index, spawn_x, spawn_y);
    return status;
}

LoomStatus loom_scene_move(const LoomInputSnapshot *input)
{
    LoomStatus status;

    if (loom_scene_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_scene_enabled == LOOM_FALSE ||
        loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        return LOOM_STATUS_OK;
    }
    status = loom_movement_update(input);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    /* Actors move after the player, so a solid actor blocks the player from
     * where it stood this tick. A controller actor reads the same pads. */
    loom_actor_set_input(input);
    return loom_actor_update();
}

LoomStatus loom_scene_dispatch_triggers(const LoomFrameBoundary *boundary,
                                        const LoomInputSnapshot *input)
{
    if (boundary == (const LoomFrameBoundary *)0 ||
        input == (const LoomInputSnapshot *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_scene_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_scene_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    if (loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        /* Every transition tick may move a counter or the phase. */
        ++loom_scene_debug_epoch;
        return loom_scene_update_transition(boundary);
    }
    return loom_scene_check_triggers(input);
}

LoomStatus loom_scene_advance_animation(void)
{
    if (loom_scene_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_scene_enabled != LOOM_FALSE &&
        loom_scene_state.transition_phase != LOOM_SCENE_TRANSITION_READY) {
        return LOOM_STATUS_OK;
    }
    return loom_animation_update();
}

LoomStatus loom_scene_update_camera(void)
{
    if (loom_scene_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    return loom_camera_update();
}

loom_u16 loom_scene_index(void)
{
    return loom_scene_state.scene_index;
}

loom_u8 loom_scene_transition_phase(void)
{
    return loom_scene_state.transition_phase;
}

loom_u16 loom_scene_transition_count(void)
{
    return loom_scene_state.transition_count;
}

loom_u16 loom_scene_trigger_dispatch_count(void)
{
    return loom_scene_state.trigger_dispatch_count;
}

loom_u16 loom_scene_collect_count(void)
{
    return loom_scene_state.collect_count;
}

loom_u16 loom_scene_black_frame_count(void)
{
    return loom_scene_state.black_frame_count;
}

void loom_scene_debug_snapshot(LoomSceneDebugSnapshot *snapshot)
{
    snapshot->scene_index = loom_scene_state.scene_index;
    snapshot->transition_count = loom_scene_state.transition_count;
    snapshot->black_frame_count = loom_scene_state.black_frame_count;
    snapshot->trigger_dispatch_count = loom_scene_state.trigger_dispatch_count;
    snapshot->collect_count = loom_scene_state.collect_count;
    snapshot->last_transition_source = loom_scene_state.last_transition_source;
    snapshot->last_transition_target = loom_scene_state.last_transition_target;
    snapshot->last_transition_spawn_x =
        loom_scene_state.last_transition_spawn_x;
    snapshot->last_transition_spawn_y =
        loom_scene_state.last_transition_spawn_y;
    snapshot->transition_phase = loom_scene_state.transition_phase;
    snapshot->player_slot = loom_scene_player_slot();
}

loom_u16 loom_scene_last_transition_source(void)
{
    return loom_scene_state.last_transition_source;
}

loom_u16 loom_scene_last_transition_target(void)
{
    return loom_scene_state.last_transition_target;
}

loom_s16 loom_scene_last_transition_spawn_x(void)
{
    return loom_scene_state.last_transition_spawn_x;
}

loom_s16 loom_scene_last_transition_spawn_y(void)
{
    return loom_scene_state.last_transition_spawn_y;
}

loom_u8 loom_scene_player_slot(void)
{
    const LoomSceneRecord *record;

    if (loom_scene_state.initialized == LOOM_FALSE ||
        loom_scene_state.scene_index == LOOM_SCENE_INVALID_INDEX) {
        return LOOM_OAM_SLOT_MAX;
    }
    record = loom_scene_record_at(loom_scene_state.scene_index);
    if (record->movement == (const LoomMovementScene *)0) {
        return LOOM_OAM_SLOT_MAX;
    }
    return record->movement->player_slot;
}
