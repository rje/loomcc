// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: actor-riders.expected-output
// loomcc-asm-sources: actor-riders.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/actor.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the solid-actor slow path. The packed solid list (add, and
// remove's swap with the last), the floor query the player's and the
// platformer bodies' landing walks (visibility, the box overlap, the highest
// deck wins), and the carry of riders when a solid actor moves. The
// movement and Mode 1 calls the unit makes are replaced by fakes here.
#include "../loom-d88b68b/runtime/src/actor.c"
int printf(const char *fmt, ...);

loom_u8 loom_movement_solid_actors;
static loom_u8 fake_visible[64];
static loom_u8 fake_riding = 0xffu;
static loom_s16 fake_player_x, fake_player_y;
const loom_u8 *loom_mode1_visibility_table(void) { return fake_visible; }
loom_u8 loom_movement_riding(void) { return fake_riding; }
void loom_movement_carry(loom_s16 dx, loom_s16 dy) {
  fake_player_x = (loom_s16)(fake_player_x + dx);
  fake_player_y = (loom_s16)(fake_player_y + dy);
}

static LoomActorType types[4];

static void show_solids(void) {
  loom_u8 i;
  printf("solids %u:", (unsigned)loom_actor_pool.solid_count);
  for (i = 0; i < loom_actor_pool.solid_count; ++i)
    printf(" %u", (unsigned)loom_actor_pool.solid_slots[i]);
  printf(" (movement %u)\n", (unsigned)loom_movement_solid_actors);
}

static void floor_query(loom_s16 l, loom_s16 r, loom_s16 t, loom_s16 b, loom_u8 self) {
  loom_s16 floor = 12345;
  loom_u8 found = loom_actor_floor_below(l, r, t, b, self, &floor);
  printf("[%d,%d]x[%d,%d] self %u: %u %d\n", l, r, t, b, (unsigned)self, (unsigned)found, floor);
}

static void show_positions(void) {
  loom_u8 i;
  for (i = 0; i < loom_actor_pool.count; ++i)
    printf("%d,%d%s", loom_actor_pool.x[i], loom_actor_pool.y[i], (i % 6 == 5) ? "\n" : " ");
  printf("player %d,%d\n", fake_player_x, fake_player_y);
}

int main(void) {
  loom_u8 i;
  types[0].box_x = 0; types[0].box_y = 0; types[0].box_width = 32; types[0].box_height = 8;
  types[1].box_x = -8; types[1].box_y = 4; types[1].box_width = 16; types[1].box_height = 16;
  types[2].box_x = 2; types[2].box_y = -6; types[2].box_width = 1; types[2].box_height = 4;
  types[3].box_x = -300; types[3].box_y = 300; types[3].box_width = 600; types[3].box_height = 1;
  loom_actor_pool.count = 12;
  loom_actor_pool.initialized = LOOM_TRUE;
  for (i = 0; i < 12; ++i) {
    loom_actor_pool.alive[i] = (loom_u8)(i != 7);
    loom_actor_pool.type_ptr[i] = &types[i & 3];
    loom_actor_pool.x[i] = (loom_s16)(i * 37 - 100);
    loom_actor_pool.y[i] = (loom_s16)(200 - i * 29);
    loom_actor_pool.sprite_index[i] = (loom_u8)(i == 5 ? 0xffu : 40u - i * 3u);
    fake_visible[40u - i * 3u] = (loom_u8)(i % 5 != 3);
    loom_actor_pool.riding[i] = LOOM_ACTOR_INVALID_INDEX;
  }
  for (i = 0; i < 12; i += 1) loom_actor_solid_add(i);
  show_solids();
  loom_actor_solid_remove(0);
  loom_actor_solid_remove(11);
  loom_actor_solid_remove(4);
  loom_actor_solid_remove(4);
  show_solids();
  loom_actor_solid_add(0);
  show_solids();

  floor_query(-200, 400, -200, 400, 0xffu);
  floor_query(-200, 400, -200, 400, 10);
  floor_query(-60, -40, 0, 300, 0xffu);
  floor_query(-70, -69, 150, 250, 0xffu);
  floor_query(0, 100, 100, 150, 2);
  floor_query(100, 90, -500, 500, 0xffu);
  floor_query(-32768, 32767, -32768, 32767, 1);
  floor_query(300, 320, -100, -1, 0xffu);
  loom_actor_pool.solid_count = 0;
  floor_query(-200, 400, -200, 400, 0xffu);
  loom_actor_pool.solid_count = 10;
  loom_actor_pool.initialized = LOOM_FALSE;
  floor_query(-200, 400, -200, 400, 0xffu);
  loom_actor_pool.initialized = LOOM_TRUE;

  loom_actor_pool.riding[1] = 3;
  loom_actor_pool.riding[2] = 3;
  loom_actor_pool.riding[7] = 3;   /* dead: stays put */
  loom_actor_pool.riding[9] = 4;
  loom_actor_pool.riding[10] = 0;
  fake_player_x = 64; fake_player_y = 96;
  show_positions();
  loom_actor_carry_riders(3, 5, -2);
  loom_actor_carry_riders(4, -300, 700);
  loom_actor_carry_riders(0, 0, 0);
  fake_riding = 3;
  loom_actor_carry_riders(3, -1, 1);
  loom_actor_carry_riders(8, 20000, -20000);
  fake_riding = 8;
  loom_actor_carry_riders(8, 20000, -20000);
  loom_actor_carry_riders(8, 2, 1);
  show_positions();
  return 0;
}
