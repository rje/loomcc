#include <loom/game.h>

/* A 16-bit xorshift with the classic 7/9/8 triple: full period over the
 * 65535 nonzero states, and three shifts per call on the 65816. */
static loom_u16 loom_rng_value = LOOM_RNG_DEFAULT_SEED;
static loom_u16 loom_game_ticks;

#if defined(LOOM_BUILD_DEBUG)
volatile loom_u16 loom_rng_debug_state = LOOM_RNG_DEFAULT_SEED;
volatile loom_u16 loom_game_debug_tick_count;
#endif

void loom_rng_seed(loom_u16 seed)
{
    loom_rng_value = seed != 0u ? seed : LOOM_RNG_DEFAULT_SEED;
#if defined(LOOM_BUILD_DEBUG)
    loom_rng_debug_state = loom_rng_value;
#endif
}

loom_u16 loom_rng_state(void)
{
    return loom_rng_value;
}

loom_u16 loom_rng_next(void)
{
    loom_u16 value;

    value = loom_rng_value;
    value = (loom_u16)(value ^ (loom_u16)(value << 7));
    value = (loom_u16)(value ^ (loom_u16)(value >> 9));
    value = (loom_u16)(value ^ (loom_u16)(value << 8));
    loom_rng_value = value;
#if defined(LOOM_BUILD_DEBUG)
    loom_rng_debug_state = value;
#endif
    return value;
}

loom_u16 loom_rng_below(loom_u16 bound)
{
    if (bound == 0u) {
        return 0u;
    }
    /* Modulo bias is at most one part in 65535 for the small bounds a game
     * asks for, and a rejection loop would cost an unbounded tick. */
    return (loom_u16)(loom_rng_next() % bound);
}

loom_u16 loom_game_tick_count(void)
{
    return loom_game_ticks;
}

void loom_game_advance_tick(void)
{
    ++loom_game_ticks;
#if defined(LOOM_BUILD_DEBUG)
    loom_game_debug_tick_count = loom_game_ticks;
#endif
}

void loom_timer_start(LoomTimer *timer, loom_u16 ticks)
{
    timer->start_tick = loom_game_ticks;
    timer->duration_ticks = ticks;
    timer->running = LOOM_TRUE;
    timer->reserved = 0u;
}

void loom_timer_stop(LoomTimer *timer)
{
    timer->running = LOOM_FALSE;
}

loom_u8 loom_timer_running(const LoomTimer *timer)
{
    return timer->running;
}

loom_u16 loom_timer_elapsed(const LoomTimer *timer)
{
    if (timer->running == LOOM_FALSE) {
        return 0u;
    }
    /* The tick clock wraps at 0xffff; a timer older than that reads as the
     * longest measurable age rather than counting backwards. */
    return (loom_u16)(loom_game_ticks - timer->start_tick);
}

loom_u8 loom_timer_expired(const LoomTimer *timer)
{
    if (timer->running == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    return (loom_u8)(loom_timer_elapsed(timer) >= timer->duration_ticks);
}
