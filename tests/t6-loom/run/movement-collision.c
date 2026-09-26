// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: movement-collision.expected-output
// loomcc-asm-sources: ../loom-d88b68b/runtime/backends/pvsneslib/src/movement.asm movement-collision.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/movement.c and runtime/backends/pvsneslib/src/movement.asm (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the collision queries every body step makes. The box test
// goes through Loom's movement.asm (C passing eight arguments, a far
// pointer among them, to hand-written assembly), then the static colliders
// and the actor bridge (faked here); plus the cell lookup through the row
// offset table, a floor within a slope or solid cell, the slope sensor,
// the column scan, and the signed 8.8 advance.
#include "../loom-d88b68b/runtime/src/movement.c"
int printf(const char *fmt, ...);

loom_u8 loom_generated_actor_blocks_box(loom_s16 left, loom_s16 top, loom_s16 right, loom_s16 bottom) {
  return (loom_u8)(left <= 215 && right >= 200 && top <= 60 && bottom >= 40);
}

#define CW 20
#define CH 12
static loom_u8 cells[CW * CH];
static const LoomStaticCollider colliders[2] = { { 100, 100, 131, 107 }, { -50, 150, 20, 400 } };
static LoomMovementScene scene;

int main(void) {
  loom_u16 i, row;
  loom_s16 x, y, f;
  loom_u8 sx, sy, sub;
  for (i = 0; i < CW * CH; i++) {
    loom_u16 cx = i % CW, cy = i / CW;
    cells[i] = (loom_u8)(cy == CH - 1 || cx == 0 ? LOOM_MOVEMENT_COLLISION_SOLID
               : ((1u * cx * 5u + cy * 3u) % 11u < 5u ? (1u * cx * 5u + cy * 3u) % 11u : 0u));
  }
  scene.pixel_width = CW * 16; scene.pixel_height = CH * 16;
  scene.collision_width = CW; scene.collision_height = CH;
  scene.collision_cells = cells;
  scene.collider_x = -6; scene.collider_y = -15; scene.collider_width = 12; scene.collider_height = 16;
  scene.static_collider_count = 2; scene.static_colliders = colliders;
  loom_movement_state.scene = &scene;
  loom_movement_grid.cells = cells;
  loom_movement_grid.pixel_width = (loom_s16)scene.pixel_width;
  loom_movement_grid.pixel_height = (loom_s16)scene.pixel_height;
  for (row = 0; row < CH; row++) loom_movement_grid.row_offsets[row] = (loom_u16)(row * CW);

  for (y = -8; y <= 200; y += 13) {
    printf("%d:", y);
    for (x = -10; x <= 330; x += 17)
      for (sx = 0; sx < 2; sx++)
        for (sy = 0; sy < 2; sy++)
          printf("%u", (unsigned)loom_movement_box_blocked(x, sx, y, (loom_u8)(sy * 128u), scene.collider_x, scene.collider_y,
                                                           scene.collider_width, scene.collider_height));
    printf("\n");
  }
  for (y = -3; y <= 195; y += 22) {
    printf("cells %d:", y);
    for (x = -5; x <= 325; x += 30) printf("%u", (unsigned)loom_movement_cell_at(x, y));
    printf(" col%u%u%u", (unsigned)loom_movement_column_solid(40, y, (loom_s16)(y + 20)),
           (unsigned)loom_movement_column_solid(170, y, (loom_s16)(y + 40)), (unsigned)loom_movement_column_solid(-1, y, y));
    printf("\n");
  }
  for (i = 0; i < 5; i++) {
    printf("floor %u:", i);
    for (x = 0; x < 40; x += 7) printf(" %d", loom_movement_floor_in_cell((loom_u8)i, x, 64));
    printf("\n");
  }
  for (x = 8; x < 320; x += 23) {
    f = -999;
    printf("slope %d:", x);
    for (y = 20; y < 190; y += 31) {
      loom_u8 r = loom_movement_slope_feet(x, y, &f);
      printf(" %u/%d", (unsigned)r, f);
    }
    printf("\n");
  }
  {
    static const loom_s16 vel[] = { 0, 1, -1, 128, -128, 256, -300, 0x7fff, -0x7fff };
    for (i = 0; i < 9; i++) {
      loom_s16 w = 100;
      sub = 0;
      for (row = 0; row < 5; row++) loom_movement_advance(&w, &sub, vel[i]);
      printf("adv %d: %d.%u\n", vel[i], w, (unsigned)sub);
    }
  }
  loom_movement_state.scene = (const LoomMovementScene *)0;
  printf("no scene %u\n", (unsigned)loom_movement_box_blocked(50, 0, 50, 0, 0, 0, 4, 4));
  return 0;
}
