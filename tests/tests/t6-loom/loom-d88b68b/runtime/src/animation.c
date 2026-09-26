#include <loom/animation.h>

typedef struct LoomAnimationState {
    loom_u8 frame_index[LOOM_ANIMATION_CAPACITY];
    loom_u8 playing[LOOM_ANIMATION_CAPACITY];
    /* The bound clip, resolved when a set player changes state or facing so
     * the per-tick loop never walks the set tables. */
    loom_u8 frame_count[LOOM_ANIMATION_CAPACITY];
    loom_u8 looping[LOOM_ANIMATION_CAPACITY];
    loom_u8 state_index[LOOM_ANIMATION_CAPACITY];
    loom_u8 direction[LOOM_ANIMATION_CAPACITY];
    loom_u8 mirror[LOOM_ANIMATION_CAPACITY];
    /* What the last drive said, packed (moving, facing x + 1, facing y + 1,
     * air): a drive that says the same again does nothing, so a body is
     * driven only when something changed, and the landing clip returns to
     * the move or idle state on its own when it ends. */
    loom_u8 drive_key[LOOM_ANIMATION_CAPACITY];
    loom_u8 alignment_padding;
    loom_u16 elapsed_ticks[LOOM_ANIMATION_CAPACITY];
    const LoomAnimationFrame *frames[LOOM_ANIMATION_CAPACITY];
    const LoomMode1SpritePose *parts[LOOM_ANIMATION_CAPACITY];
    loom_u8 part_count[LOOM_ANIMATION_CAPACITY];
    /* Player index per OAM slot, 0xff when the slot has no player. Every
     * moving actor asks twice a tick which player drives its slot; a scan
     * of the player table cost a multiply per entry on 816-tcc. */
    loom_u8 slot_player[LOOM_OAM_SLOT_MAX + 1u];
    loom_u8 initialized;
    const LoomAnimationScene *scene;
} LoomAnimationState;

static LoomAnimationState loom_animation_state;
/* The players whose set the body drives, resolved at activation: the
 * per-tick drive walks these rather than every player. */
static loom_u8 loom_animation_driven[LOOM_ANIMATION_CAPACITY];
static loom_u8 loom_animation_driven_count;

static loom_s16 loom_animation_player_index(loom_u8 sprite_slot)
{
    loom_u8 index;

    if (sprite_slot > LOOM_OAM_SLOT_MAX) {
        return (loom_s16)-1;
    }
    index = loom_animation_state.slot_player[sprite_slot];
    if (index == 0xffu) {
        return (loom_s16)-1;
    }
    return (loom_s16)index;
}

static loom_u8 loom_animation_direction_count(loom_u8 mode)
{
    switch (mode) {
    case LOOM_ANIMATION_DIRECTIONS_TWO:
        return 2u;
    case LOOM_ANIMATION_DIRECTIONS_FOUR:
        return 4u;
    case LOOM_ANIMATION_DIRECTIONS_EIGHT:
        return 8u;
    default:
        return 1u;
    }
}

/* Direction slots run clockwise from down; mirrored keeps one slot and
 * reports the flip instead. */
static loom_u8 loom_animation_direction_for(loom_u8 mode,
                                            loom_s8 facing_x,
                                            loom_s8 facing_y,
                                            loom_u8 *mirror)
{
    *mirror = LOOM_FALSE;
    switch (mode) {
    case LOOM_ANIMATION_DIRECTIONS_MIRRORED:
        if (facing_x < 0) {
            *mirror = LOOM_TRUE;
        }
        return 0u;
    case LOOM_ANIMATION_DIRECTIONS_TWO:
        return facing_x < 0 ? 1u : 0u;
    case LOOM_ANIMATION_DIRECTIONS_FOUR:
        if (facing_x > 0) {
            return 1u;
        }
        if (facing_x < 0) {
            return 3u;
        }
        return facing_y < 0 ? 2u : 0u;
    case LOOM_ANIMATION_DIRECTIONS_EIGHT:
        if (facing_x > 0) {
            if (facing_y > 0) {
                return 1u;
            }
            return facing_y < 0 ? 3u : 2u;
        }
        if (facing_x < 0) {
            if (facing_y > 0) {
                return 7u;
            }
            return facing_y < 0 ? 5u : 6u;
        }
        return facing_y < 0 ? 4u : 0u;
    default:
        return 0u;
    }
}

/* Point the player's cached clip at its set state, or at its single clip. */
static void loom_animation_bind_clip(loom_u8 player_index)
{
    const LoomAnimationPlayer *player;
    const LoomAnimationSet *set;

    player = &loom_animation_state.scene->players[player_index];
    set = player->set;
    if (set == (const LoomAnimationSet *)0) {
        loom_animation_state.frames[player_index] = player->frames;
        loom_animation_state.frame_count[player_index] = player->frame_count;
        loom_animation_state.looping[player_index] = player->looping;
        loom_animation_state.parts[player_index] = player->parts;
        loom_animation_state.part_count[player_index] = player->part_count;
        return;
    }
    {
        const LoomAnimationClip *clip;
        loom_u16 offset;

        offset = (loom_u16)(loom_animation_state.state_index[player_index] *
                            set->direction_count);
        offset = (loom_u16)(offset +
                            loom_animation_state.direction[player_index]);
        clip = &set->clips[offset];
        loom_animation_state.frames[player_index] = clip->frames;
        loom_animation_state.frame_count[player_index] = clip->frame_count;
        loom_animation_state.looping[player_index] = clip->looping;
        loom_animation_state.parts[player_index] = clip->parts;
        loom_animation_state.part_count[player_index] = clip->part_count;
    }
}

static LoomStatus loom_animation_validate_clip(const LoomAnimationClip *clip)
{
    loom_u8 frame_index;

    if (clip->frame_count == 0u || clip->looping > LOOM_TRUE ||
        clip->frames == (const LoomAnimationFrame *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (frame_index = 0u; frame_index < clip->frame_count; ++frame_index) {
        const LoomAnimationFrame *frame;

        frame = &clip->frames[frame_index];
        if (frame->duration_ticks == 0u || frame->palette > 7u ||
            frame->size > LOOM_OAM_SIZE_LARGE ||
            frame->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
            frame->width == 0u || frame->height == 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_animation_validate_set(const LoomAnimationSet *set)
{
    loom_u16 index;
    loom_u16 clip_count;

    if (set->direction_mode > LOOM_ANIMATION_DIRECTION_MODE_MAX ||
        set->direction_count !=
            loom_animation_direction_count(set->direction_mode) ||
        set->state_count == 0u ||
        set->state_count > LOOM_ANIMATION_STATE_MAX ||
        set->drive > LOOM_ANIMATION_DRIVE_MAX ||
        set->idle_state >= set->state_count ||
        set->move_state >= set->state_count ||
        set->clips == (const LoomAnimationClip *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    clip_count = (loom_u16)(set->state_count * set->direction_count);
    for (index = 0u; index < clip_count; ++index) {
        LoomStatus status;

        status = loom_animation_validate_clip(&set->clips[index]);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_animation_validate_scene(
    const LoomAnimationScene *scene)
{
    loom_u8 player_index;

    if (scene->reserved != 0u ||
        scene->player_count > LOOM_ANIMATION_CAPACITY ||
        (scene->player_count != 0u &&
         scene->players == (const LoomAnimationPlayer *)0)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (player_index = 0u; player_index < scene->player_count;
         ++player_index) {
        const LoomAnimationPlayer *player;
        loom_u8 frame_index;
        loom_u8 previous;

        player = &scene->players[player_index];
        if (player->sprite_slot > LOOM_OAM_SLOT_MAX ||
            player->frame_count == 0u ||
            player->looping > LOOM_TRUE || player->autoplay > LOOM_TRUE ||
            player->frames == (const LoomAnimationFrame *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        for (previous = 0u; previous < player_index; ++previous) {
            if (scene->players[previous].sprite_slot ==
                player->sprite_slot) {
                return LOOM_STATUS_INVALID_ARGUMENT;
            }
        }
        for (frame_index = 0u; frame_index < player->frame_count;
             ++frame_index) {
            const LoomAnimationFrame *frame;

            frame = &player->frames[frame_index];
            if (frame->duration_ticks == 0u || frame->palette > 7u ||
                frame->size > LOOM_OAM_SIZE_LARGE ||
                frame->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
                frame->width == 0u || frame->height == 0u) {
                return LOOM_STATUS_INVALID_ARGUMENT;
            }
        }
        if (player->set != (const LoomAnimationSet *)0) {
            LoomStatus status;

            status = loom_animation_validate_set(player->set);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
    return LOOM_STATUS_OK;
}

loom_u8 loom_animation_drive_generation;

static void loom_animation_reset_player(loom_u8 player_index)
{
    ++loom_animation_drive_generation;
    loom_animation_state.frame_index[player_index] = 0u;
    loom_animation_state.elapsed_ticks[player_index] = 0u;
    loom_animation_state.playing[player_index] = LOOM_FALSE;
    loom_animation_state.frame_count[player_index] = 0u;
    loom_animation_state.looping[player_index] = LOOM_FALSE;
    loom_animation_state.state_index[player_index] = 0u;
    loom_animation_state.direction[player_index] = 0u;
    loom_animation_state.mirror[player_index] = LOOM_FALSE;
    loom_animation_state.drive_key[player_index] = 0xffu;
    loom_animation_state.frames[player_index] =
        (const LoomAnimationFrame *)0;
    loom_animation_state.parts[player_index] = (const LoomMode1SpritePose *)0;
    loom_animation_state.part_count[player_index] = 0u;
}

/* Pose the player's sprite and its parts from the current frame. A mirrored
 * player reflects each sprite about its own pivot: the pivot moves to the
 * other side of the sprite's width and the X flip toggles, so a part that
 * sits to the right of the frame's pivot sits to its left when facing the
 * other way. */
static LoomStatus loom_animation_apply(loom_u8 player_index)
{
    const LoomAnimationPlayer *player;
    const LoomAnimationFrame *frame;
    LoomMode1SpritePose pose;
    loom_u8 mirror;
    loom_u8 frame_index;
    LoomStatus status;

    player = &loom_animation_state.scene->players[player_index];
    frame_index = loom_animation_state.frame_index[player_index];
    frame = &loom_animation_state.frames[player_index][frame_index];
    mirror = loom_animation_state.mirror[player_index];
    pose.pivot_x = frame->pivot_x;
    pose.pivot_y = frame->pivot_y;
    pose.tile_index = frame->tile_index;
    pose.palette = frame->palette;
    pose.size = frame->size;
    pose.width = frame->width;
    pose.height = frame->height;
    pose.flags = 0u;
    if (mirror != LOOM_FALSE) {
        pose.pivot_x = (loom_s16)((loom_s16)frame->width - frame->pivot_x);
        pose.flags = LOOM_OAM_FLAG_FLIP_X;
    }
    status = loom_mode1_set_sprite_pose(player->sprite_slot, &pose);
    if (status != LOOM_STATUS_OK ||
        loom_animation_state.part_count[player_index] == 0u) {
        return status;
    }
    {
        const LoomMode1SpritePose *part;
        loom_u8 count;
        loom_u8 index;

        count = loom_animation_state.part_count[player_index];
        part = &loom_animation_state.parts[player_index][(loom_u16)frame_index * count];
        for (index = 0u; index < count; ++index, ++part) {
            pose.pivot_x = part->pivot_x;
            pose.pivot_y = part->pivot_y;
            pose.tile_index = part->tile_index;
            pose.palette = part->palette;
            pose.size = part->size;
            pose.width = part->width;
            pose.height = part->height;
            pose.flags = part->flags;
            if (mirror != LOOM_FALSE) {
                pose.pivot_x = (loom_s16)((loom_s16)part->width - part->pivot_x);
                pose.flags = (loom_u8)(pose.flags ^ LOOM_OAM_FLAG_FLIP_X);
            }
            status = loom_mode1_set_sprite_part_pose(
                player->sprite_slot, (loom_u8)(index + 1u), &pose);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_animation_initialize(void)
{
    LoomStatus status;
    loom_u8 index;

    for (index = 0u; index < LOOM_ANIMATION_CAPACITY; ++index) {
        loom_animation_reset_player(index);
    }
    loom_animation_state.alignment_padding = 0u;
    loom_animation_state.scene = (const LoomAnimationScene *)0;
    loom_animation_state.initialized = LOOM_TRUE;
    if (loom_generated_animation_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    status = loom_animation_activate_scene(
        &loom_generated_animation_initial_scene);
    if (status != LOOM_STATUS_OK) {
        loom_animation_state.initialized = LOOM_FALSE;
    }
    return status;
}

LoomStatus loom_animation_activate_scene(const LoomAnimationScene *scene)
{
    LoomStatus status;
    loom_u8 index;

    if (loom_animation_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (scene != (const LoomAnimationScene *)0) {
        status = loom_animation_validate_scene(scene);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    loom_animation_state.scene = scene;
    loom_animation_driven_count = 0u;
#if defined(LOOM_ANIMATION_FAST)
    loom_pvs_animation_bind(loom_animation_state.playing,
                            loom_animation_state.elapsed_ticks,
                            loom_animation_state.frame_index,
                            loom_animation_state.frames);
#endif
    for (index = 0u; index < LOOM_ANIMATION_CAPACITY; ++index) {
        loom_animation_reset_player(index);
    }
    for (index = 0u; index < LOOM_OAM_SLOT_MAX; ++index) {
        loom_animation_state.slot_player[index] = 0xffu;
    }
    loom_animation_state.slot_player[LOOM_OAM_SLOT_MAX] = 0xffu;
    if (scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_OK;
    }
    {
        const LoomAnimationPlayer *player;

        player = scene->players;
        for (index = 0u; index < scene->player_count; ++index, ++player) {
            loom_animation_state.slot_player[player->sprite_slot] = index;
            loom_animation_state.playing[index] = player->autoplay;
            if (player->set != (const LoomAnimationSet *)0 &&
                player->set->drive == LOOM_ANIMATION_DRIVE_MOVEMENT) {
                loom_animation_driven[loom_animation_driven_count] = index;
                ++loom_animation_driven_count;
            }
            loom_animation_bind_clip(index);
            status = loom_animation_apply(index);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
    return LOOM_STATUS_OK;
}

/* A player whose frame has run its duration: the next frame, the loop, or
 * the end -- where a landing clip returns to the ground state on its own. */
static LoomStatus loom_animation_advance_player(loom_u8 index)
{
    LoomStatus status;

    {
        loom_animation_state.elapsed_ticks[index] = 0u;
        if ((loom_u8)(loom_animation_state.frame_index[index] + 1u) <
            loom_animation_state.frame_count[index]) {
            ++loom_animation_state.frame_index[index];
        } else if (loom_animation_state.looping[index] != LOOM_FALSE) {
            loom_animation_state.frame_index[index] = 0u;
        } else {
            const LoomAnimationSet *set;

            loom_animation_state.playing[index] = LOOM_FALSE;
            /* A landing clip that has played out returns to the ground
             * state the last drive asked for, so no body has to drive its
             * set every tick to notice. */
            set = loom_animation_state.scene->players[index].set;
            if (set != (const LoomAnimationSet *)0 &&
                set->land_state != LOOM_ANIMATION_INVALID_STATE &&
                loom_animation_state.state_index[index] == set->land_state) {
                loom_animation_state.state_index[index] =
                    (loom_animation_state.drive_key[index] & 1u) != 0u
                        ? set->move_state
                        : set->idle_state;
                loom_animation_state.frame_index[index] = 0u;
                loom_animation_state.playing[index] = LOOM_TRUE;
                loom_animation_bind_clip(index);
                status = loom_animation_apply(index);
                if (status != LOOM_STATUS_OK) {
                    return status;
                }
            }
            return LOOM_STATUS_OK;
        }
        status = loom_animation_apply(index);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

#if defined(LOOM_ANIMATION_FAST)
LOOM_STATIC_ASSERT(loom_animation_frame_matches_body_asm,
                   sizeof(LoomAnimationFrame) == LOOM_ANIMATION_FRAME_BYTES);
/* The players the pass found due this tick. */
static loom_u8 loom_animation_due[LOOM_ANIMATION_CAPACITY];
#endif

LoomStatus loom_animation_update(void)
{
    loom_u8 index;

    if (loom_animation_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_OK;
    }
#if defined(LOOM_ANIMATION_FAST)
    (void)index;
    {
        loom_u16 due;
        loom_u16 position;

        due = loom_pvs_animation_pass(loom_animation_state.scene->player_count,
                                      loom_animation_due);
        for (position = 0u; position < due; ++position) {
            LoomStatus status;

            status = loom_animation_advance_player(loom_animation_due[position]);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
#else
    {
        const LoomAnimationFrame **frames;

        /* A pointer walk over the players: an indexed struct access is a
         * multiply helper call on 816-tcc. */
        frames = loom_animation_state.frames;
        for (index = 0u;
             index < loom_animation_state.scene->player_count;
             ++index, ++frames) {
            const LoomAnimationFrame *frame;
            LoomStatus status;

            if (loom_animation_state.playing[index] == LOOM_FALSE) {
                continue;
            }
            frame = &(*frames)[loom_animation_state.frame_index[index]];
            ++loom_animation_state.elapsed_ticks[index];
            if (loom_animation_state.elapsed_ticks[index] <
                frame->duration_ticks) {
                continue;
            }
            status = loom_animation_advance_player(index);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
#endif
    return LOOM_STATUS_OK;
}

LoomStatus loom_animation_play(loom_u8 sprite_slot, loom_u8 restart)
{
    loom_s16 index;
    LoomStatus status;

    if (restart > LOOM_TRUE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_animation_player_index(sprite_slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (restart != LOOM_FALSE) {
        loom_animation_state.frame_index[(loom_u8)index] = 0u;
        loom_animation_state.elapsed_ticks[(loom_u8)index] = 0u;
        status = loom_animation_apply((loom_u8)index);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    loom_animation_state.playing[(loom_u8)index] = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

static LoomStatus loom_animation_select_player(loom_u8 player_index,
                                               const LoomAnimationSet *set,
                                               loom_u8 state,
                                               loom_s8 facing_x,
                                               loom_s8 facing_y)
{
    loom_u8 direction;
    loom_u8 mirror;

    if (state >= set->state_count) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    direction = loom_animation_direction_for(set->direction_mode, facing_x,
                                             facing_y, &mirror);
    if (loom_animation_state.state_index[player_index] == state &&
        loom_animation_state.direction[player_index] == direction &&
        loom_animation_state.mirror[player_index] == mirror &&
        loom_animation_state.playing[player_index] != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    loom_animation_state.state_index[player_index] = state;
    loom_animation_state.direction[player_index] = direction;
    loom_animation_state.mirror[player_index] = mirror;
    loom_animation_state.frame_index[player_index] = 0u;
    loom_animation_state.elapsed_ticks[player_index] = 0u;
    loom_animation_state.playing[player_index] = LOOM_TRUE;
    loom_animation_bind_clip(player_index);
    return loom_animation_apply(player_index);
}

LoomStatus loom_animation_select(loom_u8 sprite_slot,
                                 loom_u8 state,
                                 loom_s8 facing_x,
                                 loom_s8 facing_y)
{
    const LoomAnimationSet *set;
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_animation_player_index(sprite_slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    set = loom_animation_state.scene->players[(loom_u8)index].set;
    if (set == (const LoomAnimationSet *)0) {
        /* A single authored clip has no states to select. */
        return LOOM_STATUS_OK;
    }
    return loom_animation_select_player((loom_u8)index, set, state, facing_x,
                                        facing_y);
}

/* The state a movement-driven set plays for a body: the landing clip on the
 * tick it lands and until that clip ends, the air states while it is off
 * the ground (whichever of jump and fall the set has), else move or idle. */
static loom_u8 loom_animation_state_for(loom_u8 player_index,
                                        const LoomAnimationSet *set,
                                        loom_u8 moving,
                                        loom_u8 air)
{
    loom_u8 state;

    if (set->land_state != LOOM_ANIMATION_INVALID_STATE) {
        if (air == LOOM_BODY_AIR_LANDED) {
            return set->land_state;
        }
        if (air == LOOM_BODY_AIR_GROUND &&
            loom_animation_state.state_index[player_index] == set->land_state &&
            loom_animation_state.looping[player_index] == LOOM_FALSE &&
            loom_animation_state.playing[player_index] != LOOM_FALSE) {
            return set->land_state;
        }
    }
    if (air == LOOM_BODY_AIR_RISING || air == LOOM_BODY_AIR_FALLING) {
        state = air == LOOM_BODY_AIR_RISING ? set->jump_state : set->fall_state;
        if (state == LOOM_ANIMATION_INVALID_STATE) {
            state = air == LOOM_BODY_AIR_RISING ? set->fall_state
                                                : set->jump_state;
        }
        if (state != LOOM_ANIMATION_INVALID_STATE) {
            return state;
        }
    }
    return moving != LOOM_FALSE ? set->move_state : set->idle_state;
}

/* One drive of a set player. The same request as last time changes
 * nothing -- the set can only move on from a finished landing clip, and
 * the update handles that -- so it costs a compare and returns. */
static LoomStatus loom_animation_drive_player(loom_u8 player_index,
                                              const LoomAnimationSet *set,
                                              loom_u8 moving,
                                              loom_s8 facing_x,
                                              loom_s8 facing_y,
                                              loom_u8 air)
{
    loom_u8 key;

    key = (loom_u8)((moving != LOOM_FALSE ? 1u : 0u) |
                    ((loom_u8)(facing_x + 1) << 1) |
                    ((loom_u8)(facing_y + 1) << 3) | (loom_u8)(air << 5));
    if (key == loom_animation_state.drive_key[player_index]) {
        return LOOM_STATUS_OK;
    }
    loom_animation_state.drive_key[player_index] = key;
    return loom_animation_select_player(
        player_index, set,
        loom_animation_state_for(player_index, set, moving, air), facing_x,
        facing_y);
}

LoomStatus loom_animation_drive_slot(loom_u8 sprite_slot,
                                     loom_u8 moving,
                                     loom_s8 facing_x,
                                     loom_s8 facing_y,
                                     loom_u8 air)
{
    const LoomAnimationSet *set;
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    /* A room with no animation players has nothing to drive either: an
     * actor drawn with a still frame must not stop the tick. */
    if (loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_OK;
    }
    index = loom_animation_player_index(sprite_slot);
    if (index < 0) {
        /* A slot without a player simply has nothing to drive. */
        return LOOM_STATUS_OK;
    }
    set = loom_animation_state.scene->players[(loom_u8)index].set;
    if (set == (const LoomAnimationSet *)0) {
        return LOOM_STATUS_OK;
    }
    return loom_animation_drive_player((loom_u8)index, set, moving, facing_x,
                                       facing_y, air);
}

LoomStatus loom_animation_drive(loom_s8 facing_x,
                                loom_s8 facing_y,
                                loom_u8 moving,
                                loom_u8 air)
{
    const LoomAnimationPlayer *players;
    loom_u8 position;
    loom_u8 key;

    if (loom_animation_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_OK;
    }
    players = loom_animation_state.scene->players;
    /* Every driven player hears the same movement, so the key is worked out
     * once and a player already on it costs a compare, not a six-argument
     * call (most ticks change nothing). */
    key = (loom_u8)((moving != LOOM_FALSE ? 1u : 0u) |
                    ((loom_u8)(facing_x + 1) << 1) |
                    ((loom_u8)(facing_y + 1) << 3) | (loom_u8)(air << 5));
    for (position = 0u; position < loom_animation_driven_count; ++position) {
        loom_u8 index;
        LoomStatus status;

        index = loom_animation_driven[position];
        if (loom_animation_state.drive_key[index] == key) {
            continue;
        }
        status = loom_animation_drive_player(index, players[index].set, moving,
                                             facing_x, facing_y, air);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_animation_drive_key(loom_u8 key)
{
    return loom_animation_drive(
        (loom_s8)((loom_s8)((key >> 1) & 3u) - 1),
        (loom_s8)((loom_s8)((key >> 3) & 3u) - 1), (loom_u8)(key & 1u),
        (loom_u8)(key >> 5));
}

void loom_animation_debug_snapshot(loom_u8 sprite_slot,
                                   LoomAnimationDebugSnapshot *snapshot)
{
    loom_s16 index;

    snapshot->frame_index = LOOM_ANIMATION_INVALID_FRAME;
    snapshot->state = LOOM_ANIMATION_INVALID_STATE;
    snapshot->direction = LOOM_ANIMATION_INVALID_STATE;
    snapshot->playing = LOOM_FALSE;
    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return;
    }
    index = loom_animation_player_index(sprite_slot);
    if (index < 0) {
        return;
    }
    snapshot->frame_index = loom_animation_state.frame_index[(loom_u8)index];
    snapshot->state = loom_animation_state.state_index[(loom_u8)index];
    snapshot->direction = loom_animation_state.direction[(loom_u8)index];
    snapshot->playing = loom_animation_state.playing[(loom_u8)index];
}

loom_u8 loom_animation_state_index(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_ANIMATION_INVALID_STATE;
    }
    index = loom_animation_player_index(sprite_slot);
    return index < 0 ? LOOM_ANIMATION_INVALID_STATE
                     : loom_animation_state.state_index[(loom_u8)index];
}

loom_u8 loom_animation_direction(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_ANIMATION_INVALID_STATE;
    }
    index = loom_animation_player_index(sprite_slot);
    return index < 0 ? LOOM_ANIMATION_INVALID_STATE
                     : loom_animation_state.direction[(loom_u8)index];
}

LoomStatus loom_animation_stop(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_animation_player_index(sprite_slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    loom_animation_state.playing[(loom_u8)index] = LOOM_FALSE;
    return LOOM_STATUS_OK;
}

loom_u8 loom_animation_frame_index(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_ANIMATION_INVALID_FRAME;
    }
    index = loom_animation_player_index(sprite_slot);
    return index < 0 ? LOOM_ANIMATION_INVALID_FRAME
                     : loom_animation_state.frame_index[(loom_u8)index];
}

loom_u16 loom_animation_elapsed_ticks(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return 0u;
    }
    index = loom_animation_player_index(sprite_slot);
    return index < 0 ? 0u
                     : loom_animation_state.elapsed_ticks[(loom_u8)index];
}

loom_u8 loom_animation_playing(loom_u8 sprite_slot)
{
    loom_s16 index;

    if (loom_animation_state.initialized == LOOM_FALSE ||
        loom_animation_state.scene == (const LoomAnimationScene *)0) {
        return LOOM_FALSE;
    }
    index = loom_animation_player_index(sprite_slot);
    return index < 0 ? LOOM_FALSE
                     : loom_animation_state.playing[(loom_u8)index];
}
