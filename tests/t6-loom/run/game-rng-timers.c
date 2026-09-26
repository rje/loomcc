// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: game-rng-timers.expected-output
// loomcc-ref: tcc-rom
// loomcc-source: Loom d88b68b runtime/src/game.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: Loom's RNG and timers, driven through a fixed sequence of calls;
// the expected output is what the same program prints when 816-tcc builds it
// (scripts/rom-output.py), so loomcc must agree with Loom's current compiler.
// u8 results are cast to unsigned for printf: 816-tcc does not promote char
// variadic arguments (docs/TCC-BUGS.md).
#include "../loom-d88b68b/runtime/src/game.c"
int printf(const char *fmt, ...);
int main(void) {
  static const loom_u16 seeds[] = { 0, 1, 0x1234, 0xffff, 0x8000 };
  static const loom_u16 bounds[] = { 0, 1, 2, 7, 10, 100, 1000, 65535u };
  LoomTimer t;
  loom_u8 i, j;
  for (i = 0; i < 5; i++) {
    loom_rng_seed(seeds[i]);
    printf("seed %u state %u:", seeds[i], loom_rng_state());
    for (j = 0; j < 6; j++) printf(" %u", loom_rng_next());
    for (j = 0; j < 8; j++) printf(" b%u=%u", bounds[j], loom_rng_below(bounds[j]));
    printf("\n");
  }
  for (i = 0; i < 5; i++) loom_game_advance_tick();
  loom_timer_start(&t, 3);
  for (i = 0; i < 6; i++) {
    printf("tick %u running %u elapsed %u expired %u\n", loom_game_tick_count(), (unsigned)loom_timer_running(&t), loom_timer_elapsed(&t), (unsigned)loom_timer_expired(&t));
    loom_game_advance_tick();
  }
  loom_timer_stop(&t);
  printf("stopped running %u expired %u\n", (unsigned)loom_timer_running(&t), (unsigned)loom_timer_expired(&t));
  return 0;
}
