/* The Loom types animation_pass needs, copied from Loom 5184be9^
 * (runtime/include/loom/types.h, pools.h, animation.h and the state struct
 * in runtime/src/animation.c). LOOM_ANIMATION_CAPACITY is the host default
 * pool size (33). The assembly reads LoomAnimationFrame by offset. */
#ifndef ANIMATION_PASS_LOOM_TYPES_H
#define ANIMATION_PASS_LOOM_TYPES_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_OAM_SLOT_MAX ((loom_u8)127u)
#define LOOM_FRAME_OAM_CAPACITY ((loom_u8)33u)
#define LOOM_ANIMATION_CAPACITY LOOM_FRAME_OAM_CAPACITY

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

typedef struct LoomMode1SpritePose LoomMode1SpritePose;
typedef struct LoomAnimationScene LoomAnimationScene;

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
    loom_u8 drive_key[LOOM_ANIMATION_CAPACITY];
    loom_u8 alignment_padding;
    loom_u16 elapsed_ticks[LOOM_ANIMATION_CAPACITY];
    const LoomAnimationFrame *frames[LOOM_ANIMATION_CAPACITY];
    const LoomMode1SpritePose *parts[LOOM_ANIMATION_CAPACITY];
    loom_u8 part_count[LOOM_ANIMATION_CAPACITY];
    loom_u8 slot_player[LOOM_OAM_SLOT_MAX + 1u];
    loom_u8 initialized;
    const LoomAnimationScene *scene;
} LoomAnimationState;

#ifdef __65816__
/* body.asm reads the frame by offset: duration_ticks at 6, 12 bytes. */
typedef char animation_pass_frame_is_12[(sizeof(LoomAnimationFrame) == 12u) ? 1 : -1];
#endif

/* Owned by the driver (animation.c's static in Loom). */
extern LoomAnimationState loom_animation_state;

void loom_pvs_animation_bind(loom_u8 *playing, loom_u16 *elapsed_ticks,
                             loom_u8 *frame_index,
                             const LoomAnimationFrame **frames);
loom_u16 loom_pvs_animation_pass(loom_u16 count, loom_u8 *due);

#endif
