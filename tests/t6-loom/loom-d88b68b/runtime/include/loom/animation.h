#ifndef LOOM_ANIMATION_H
#define LOOM_ANIMATION_H

#include <loom/mode1.h>
#include <loom/movement.h>

#define LOOM_ANIMATION_CAPACITY LOOM_FRAME_OAM_CAPACITY
#define LOOM_ANIMATION_INVALID_FRAME ((loom_u8)0xffu)
#define LOOM_ANIMATION_INVALID_STATE ((loom_u8)0xffu)
#define LOOM_ANIMATION_STATE_MAX ((loom_u8)16u)

/*
 * How an animation set maps facing to clips. Direction slots run clockwise
 * from down, so four-way is down, right, up, left and eight-way inserts the
 * diagonals. Mirrored authors the right-facing clip once and flips it.
 */
#define LOOM_ANIMATION_DIRECTIONS_NONE ((loom_u8)0u)
#define LOOM_ANIMATION_DIRECTIONS_MIRRORED ((loom_u8)1u)
#define LOOM_ANIMATION_DIRECTIONS_TWO ((loom_u8)2u)
#define LOOM_ANIMATION_DIRECTIONS_FOUR ((loom_u8)3u)
#define LOOM_ANIMATION_DIRECTIONS_EIGHT ((loom_u8)4u)
#define LOOM_ANIMATION_DIRECTION_MODE_MAX LOOM_ANIMATION_DIRECTIONS_EIGHT

typedef struct LoomAnimationFrame {
    loom_s16 pivot_x;
    loom_s16 pivot_y;
    loom_u16 tile_index;
    loom_u16 duration_ticks;
    loom_u8 palette;
    loom_u8 size;
    loom_u8 width;
    loom_u8 height;
} LoomAnimationFrame;

/* One named state's clip for one direction slot. */
typedef struct LoomAnimationClip {
    loom_u8 frame_count;
    loom_u8 looping;
    const LoomAnimationFrame *frames;
    /* The poses of the metasprite's parts, frame-major: frame f's part p is
     * parts[f * part_count + p]. Null with a part_count of zero for a frame
     * of one hardware sprite. Each pose's pivot is the part's own (the
     * frame's pivot less the part's offset); mirrored play flips the pivot
     * about the part's width and toggles its X flip. */
    const LoomMode1SpritePose *parts;
    loom_u8 part_count;
} LoomAnimationClip;

/*
 * A generated animation set: user-named states by direction. Clips are
 * state-major, so state s in direction d is clips[s * direction_count + d].
 * State identities are generated ordinals; game code uses the generated
 * LOOM_ANIMATION_STATE_<SET>_<NAME> constants.
 */
/* How the runtime picks a state for a set each tick. */
#define LOOM_ANIMATION_DRIVE_NONE ((loom_u8)0u)
#define LOOM_ANIMATION_DRIVE_MOVEMENT ((loom_u8)1u)
#define LOOM_ANIMATION_DRIVE_MAX LOOM_ANIMATION_DRIVE_MOVEMENT

typedef struct LoomAnimationSet {
    loom_u8 direction_mode;
    loom_u8 direction_count;
    loom_u8 state_count;
    /* LOOM_ANIMATION_DRIVE_MOVEMENT selects move_state while the actor
     * moves and idle_state while it rests; game code owns the rest. */
    loom_u8 drive;
    loom_u8 idle_state;
    loom_u8 move_state;
    /* A platformer body's states, or LOOM_ANIMATION_INVALID_STATE when the
     * set has none: rising, falling, and the clip that plays out on
     * landing before the ground states resume (author it non-looping). */
    loom_u8 jump_state;
    loom_u8 fall_state;
    loom_u8 land_state;
    const LoomAnimationClip *clips;
} LoomAnimationSet;

typedef struct LoomAnimationPlayer {
    loom_u8 sprite_slot;
    loom_u8 frame_count;
    loom_u8 looping;
    loom_u8 autoplay;
    const LoomAnimationFrame *frames;
    /* Null for a single authored clip; otherwise frames/frame_count/looping
     * describe the set's first state in its first direction. */
    const LoomAnimationSet *set;
    /* The clip's part poses, as on LoomAnimationClip. */
    const LoomMode1SpritePose *parts;
    loom_u8 part_count;
} LoomAnimationPlayer;

typedef struct LoomAnimationScene {
    loom_u8 player_count;
    loom_u8 reserved;
    const LoomAnimationPlayer *players;
} LoomAnimationScene;

/* Defined by generated mode1_data.c for the selected initial scene. */
extern const loom_u8 loom_generated_animation_enabled;
extern const LoomAnimationScene loom_generated_animation_initial_scene;

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
#define LOOM_ANIMATION_FAST 1
/* body.asm's per-player tick: the elapsed ticks count up and the players
 * whose frame ran its duration land on `due`; C advances those. */
void loom_pvs_animation_bind(loom_u8 *playing, loom_u16 *elapsed_ticks,
                             loom_u8 *frame_index,
                             const LoomAnimationFrame **frames);
loom_u16 loom_pvs_animation_pass(loom_u16 count, loom_u8 *due);
/* body.asm reads the frame by offset: duration_ticks at 6. */
#define LOOM_ANIMATION_FRAME_BYTES 12u
#endif
LoomStatus loom_animation_initialize(void);
LoomStatus loom_animation_activate_scene(const LoomAnimationScene *scene);
LoomStatus loom_animation_update(void);
LoomStatus loom_animation_play(loom_u8 sprite_slot, loom_u8 restart);
/*
 * Select a set player's state and facing. The clip restarts whenever the
 * state, the direction slot, or the mirror flag changes; a player without a
 * set ignores the request. Facing components are -1, 0 or 1.
 */
LoomStatus loom_animation_select(loom_u8 sprite_slot,
                                 loom_u8 state,
                                 loom_s8 facing_x,
                                 loom_s8 facing_y);
/*
 * Select states for every movement-driven set in the active scene. The
 * generated schedule passes the movement body's facing and whether it moved
 * this tick.
 */
LoomStatus loom_animation_drive(loom_s8 facing_x,
                                loom_s8 facing_y,
                                loom_u8 moving,
                                loom_u8 air);
/* The same from the packed key loom_movement_animation_key() returns:
 * moving | (facing_x + 1) << 1 | (facing_y + 1) << 3 | air << 5. */
LoomStatus loom_animation_drive_key(loom_u8 key);
/* Bumped whenever a player's drive state is reset (a scene activating), so
 * a caller that skips driving while the key is unchanged knows to drive
 * again. */
extern loom_u8 loom_animation_drive_generation;
/*
 * Select the idle or move state of one slot's set from a caller that owns
 * the motion, which is how the actor pool animates its instances.
 */
LoomStatus loom_animation_drive_slot(loom_u8 sprite_slot,
                                     loom_u8 moving,
                                     loom_s8 facing_x,
                                     loom_s8 facing_y,
                                     loom_u8 air);
loom_u8 loom_animation_state_index(loom_u8 sprite_slot);
loom_u8 loom_animation_direction(loom_u8 sprite_slot);

/* One slot lookup for the debug witness instead of one per field. */
typedef struct LoomAnimationDebugSnapshot {
    loom_u8 frame_index;
    loom_u8 state;
    loom_u8 direction;
    loom_u8 playing;
} LoomAnimationDebugSnapshot;

void loom_animation_debug_snapshot(loom_u8 sprite_slot,
                                   LoomAnimationDebugSnapshot *snapshot);
LoomStatus loom_animation_stop(loom_u8 sprite_slot);
loom_u8 loom_animation_frame_index(loom_u8 sprite_slot);
loom_u16 loom_animation_elapsed_ticks(loom_u8 sprite_slot);
loom_u8 loom_animation_playing(loom_u8 sprite_slot);

#endif
