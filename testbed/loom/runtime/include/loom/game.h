#ifndef LOOM_GAME_H
#define LOOM_GAME_H

#include <loom/types.h>

/*
 * Small deterministic helpers a game hook needs before actors exist: a
 * 16-bit generator and tick timers. Both are fixed storage, and both are
 * driven by the same logical tick the schedule counts, so a replay of the
 * same inputs from the same seed reproduces exactly.
 */

#define LOOM_RNG_DEFAULT_SEED ((loom_u16)0xace1u)

/* Reseeds the generator. A zero seed would lock a xorshift at zero, so it
 * is replaced with the default. */
void loom_rng_seed(loom_u16 seed);
/* The current state, which is the next value the generator will return. */
loom_u16 loom_rng_state(void);
/* One 16-bit xorshift step. */
loom_u16 loom_rng_next(void);
/* A value from 0 through bound - 1; a zero bound returns zero. */
loom_u16 loom_rng_below(loom_u16 bound);

typedef struct LoomTimer {
    loom_u16 start_tick;
    loom_u16 duration_ticks;
    loom_u8 running;
    loom_u8 reserved;
} LoomTimer;

/* Ticks since the runtime started, which is what timers measure against. */
loom_u16 loom_game_tick_count(void);
/* Advances the tick clock. The generated schedule owns this call. */
void loom_game_advance_tick(void);

void loom_timer_start(LoomTimer *timer, loom_u16 ticks);
void loom_timer_stop(LoomTimer *timer);
loom_u8 loom_timer_running(const LoomTimer *timer);
/* Ticks since the timer started, saturating at 0xffff. */
loom_u16 loom_timer_elapsed(const LoomTimer *timer);
/* True once a running timer has reached its duration. */
loom_u8 loom_timer_expired(const LoomTimer *timer);

#if defined(LOOM_BUILD_DEBUG)
/* Watchable by symbol in the Game tab so a ROM test can pin a seed. */
extern volatile loom_u16 loom_rng_debug_state;
extern volatile loom_u16 loom_game_debug_tick_count;
#endif

#endif
