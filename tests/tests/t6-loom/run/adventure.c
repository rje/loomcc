// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: adventure.expected-output
// loomcc-ref: tcc-rom
// loomcc-source: Loom d88b68b runtime/src/adventure.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: Loom's adventure flags, gates, actions and request queue,
// compared with the 816-tcc build of the same program.
#include "../loom-d88b68b/runtime/src/adventure.c"
int printf(const char *fmt, ...);
static void show(const char *what, LoomStatus s) { printf("%s=%u ", what, (unsigned)s); }
int main(void) {
  LoomInputSnapshot in;
  LoomAdventureRequest req;
  loom_u8 f, d = 0, i;
  show("set-early", loom_adventure_flag_set(1));
  show("init", loom_adventure_initialize());
  printf("\n");
  for (f = 0; f < 18; f += 3) { show("set", loom_adventure_flag_set(f)); }
  show("clear", loom_adventure_flag_clear(6));
  printf("\nflags=%u", loom_adventure_flags());
  for (f = 0; f < 18; f++) printf(" %u", (unsigned)loom_adventure_flag_is_set(f));
  printf("\n");
  for (f = 0; f < 18; f += 4) printf("gate %u %u %u %u\n", (unsigned)f, (unsigned)loom_adventure_gate_allows(f, 0), (unsigned)loom_adventure_gate_allows(f, 1), (unsigned)loom_adventure_gate_allows(f, 2));
  printf("gate none %u\n", (unsigned)loom_adventure_gate_allows(LOOM_ADVENTURE_FLAG_NONE, 1));
  in.frame_id = 0; in.pad_count = 1; in.reserved = 0;
  in.pads[0].pressed = 0;
  for (i = 0; i < 7; i++) {
    LoomStatus s = loom_adventure_try_action(LOOM_ADVENTURE_ACTION_PICKUP, (loom_u8)(i + 1), (loom_u8)(10 + i), &in, (loom_u8)(i != 2), &d);
    printf("pickup %u -> %u dispatched %u pending %u\n", (unsigned)i, (unsigned)s, (unsigned)d, (unsigned)loom_adventure_pending_request_count());
  }
  while (loom_adventure_take_request(&req) == LOOM_STATUS_OK) printf("take %u %u\n", (unsigned)req.kind, (unsigned)req.id);
  printf("count %u last %u %u\n", loom_adventure_request_count(), (unsigned)loom_adventure_last_request_kind(), (unsigned)loom_adventure_last_request_id());
  return 0;
}
