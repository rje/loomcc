/* animation_pass: loom_animation_update's per-player tick over nine
 * animation players -- a walking hero (4 frames of 6 ticks), a coin (8 of
 * 4), a Goomba (2 of 8), a torch (3 of 1..3), a stopped clip, a one-frame
 * idle (duration 60), two more walkers and a stopped player -- run for
 * five ticks. After each pass the driver advances the due players as C's
 * loom_animation_advance_player would (elapsed to zero, the next frame,
 * looping), so the frames and due lists change tick to tick. */
#include "bench.h"
#include "loom_types.h"

#define PLAYERS 9
#define TICKS 5

LoomAnimationState loom_animation_state;

#define F(d) {-8, -16, 0x40u, (d), 1u, 1u, 16u, 16u}
static const LoomAnimationFrame hero[4] = {F(6), F(6), F(6), F(6)};
static const LoomAnimationFrame coin[8] = {F(4), F(4), F(4), F(4), F(4), F(4), F(4), F(4)};
static const LoomAnimationFrame goomba[2] = {F(8), F(8)};
static const LoomAnimationFrame torch[3] = {F(1), F(3), F(2)};
static const LoomAnimationFrame idle[1] = {F(60)};

static const LoomAnimationFrame *const clip_frames[PLAYERS] = {
    hero, coin, goomba, torch, hero, idle, goomba, coin, torch};
static const loom_u8 clip_counts[PLAYERS] = {4u, 8u, 2u, 3u, 4u, 1u, 2u, 8u, 3u};
static const loom_u8 clip_playing[PLAYERS] = {1u, 1u, 1u, 1u, 0u, 1u, 1u, 1u, 0u};
static const loom_u8 clip_start[PLAYERS] = {0u, 3u, 1u, 0u, 2u, 0u, 0u, 7u, 1u};
static const loom_u16 clip_elapsed[PLAYERS] = {2u, 3u, 0u, 0u, 1u, 55u, 7u, 1u, 0u};

static loom_u8 due[TICKS][LOOM_ANIMATION_CAPACITY];
static loom_u16 due_count[TICKS];

void bench_setup(void)
{
    unsigned short i;

    for (i = 0; i < PLAYERS; i++) {
        loom_animation_state.frames[i] = clip_frames[i];
        loom_animation_state.frame_count[i] = clip_counts[i];
        loom_animation_state.looping[i] = 1u;
        loom_animation_state.playing[i] = clip_playing[i];
        loom_animation_state.frame_index[i] = clip_start[i];
        loom_animation_state.elapsed_ticks[i] = clip_elapsed[i];
    }
    loom_animation_state.initialized = 1u;
    loom_pvs_animation_bind(loom_animation_state.playing,
                            loom_animation_state.elapsed_ticks,
                            loom_animation_state.frame_index,
                            loom_animation_state.frames);
}

/* What C does with the due list: the next frame, looping. */
static void advance(const loom_u8 *list, loom_u16 count)
{
    loom_u16 k;
    loom_u8 p, next;

    for (k = 0; k < count; k++) {
        p = list[k];
        loom_animation_state.elapsed_ticks[p] = 0u;
        next = (loom_u8)(loom_animation_state.frame_index[p] + 1u);
        loom_animation_state.frame_index[p] =
            next < loom_animation_state.frame_count[p] ? next : 0u;
    }
}

#define TICK(t) \
    due_count[t] = loom_pvs_animation_pass(PLAYERS, due[t]); \
    advance(due[t], due_count[t])

void bench_run(void)
{
    TICK(0);
    TICK(1);
    TICK(2);
    TICK(3);
    TICK(4);
}

void bench_check(void)
{
    unsigned short t, k, h;

    for (t = 0; t < TICKS; t++) {
        h = due_count[t];
        for (k = 0; k < due_count[t]; k++)
            h = (unsigned short)((h << 4) ^ (h >> 12) ^ due[t][k]);
        BENCH_OUT(due_count[t]);
        BENCH_OUT(h);
    }
    for (k = 0; k < PLAYERS; k++)
        BENCH_OUT(loom_animation_state.elapsed_ticks[k] |
                  (loom_animation_state.frame_index[k] << 8));
}
