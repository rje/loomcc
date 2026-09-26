// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: scene-check.expected-output
// loomcc-asm-sources: ../loom-d88b68b/runtime/backends/pvsneslib/src/scene.asm scene-check.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/scene.c and runtime/backends/pvsneslib/src/scene.asm (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the scene's per-tick trigger check (loom_scene_check_triggers):
// the idle-tick shortcut, visibility re-application on a flag change,
// Loom's scene.asm box scan, gates, once-per-entry and exit triggers,
// collectibles, and hook-range dispatch. The generated adventure kit, the
// movement position and the Mode 1 calls are fakes that log what they get.
#include "../loom-d88b68b/runtime/src/scene.c"
int printf(const char *fmt, ...);

const loom_u16 loom_generated_scene_count = 3;
static loom_u16 fake_epoch;
static loom_s16 fake_x, fake_y;
static loom_u16 applied;
loom_s16 loom_movement_player_x(void) { return fake_x; }
loom_s16 loom_movement_player_y(void) { return fake_y; }
loom_u16 loom_generated_adventure_visibility_epoch(void) { return fake_epoch; }
LoomStatus loom_generated_adventure_apply_visibility(const LoomSceneTrigger *t) {
  applied = (loom_u16)(applied + t->sprite_slot + 1u);
  return LOOM_STATUS_OK;
}
loom_u8 loom_generated_adventure_gate_allows(const LoomSceneTrigger *t) {
  return (loom_u8)(t->adventure_gate_flag != 7u || (fake_epoch & 1u) != 0u);
}
LoomStatus loom_generated_adventure_try_action(const LoomSceneTrigger *t, const LoomInputSnapshot *input,
                                               loom_u8 entering, loom_u8 *dispatched) {
  *dispatched = (loom_u8)(t->adventure_action != LOOM_ADVENTURE_ACTION_NONE &&
                          (entering != LOOM_FALSE || (input->pads[0].held & 0x80u) != 0u));
  printf(" a%u%c", (unsigned)t->adventure_flag, *dispatched ? '+' : '-');
  return LOOM_STATUS_OK;
}
LoomStatus loom_generated_dispatch_user_hook(loom_u16 id) {
  printf(" h%u", id);
  return id == 999u ? LOOM_STATUS_INVALID_ARGUMENT : LOOM_STATUS_OK;
}
LoomStatus loom_mode1_set_sprite_visible(loom_u8 slot, loom_u8 visible) {
  printf(" s%u=%u", (unsigned)slot, (unsigned)visible);
  return LOOM_STATUS_OK;
}
LoomStatus loom_mode1_set_raster_enabled(loom_u8 enabled) {
  printf(" r%u", (unsigned)enabled);
  return LOOM_STATUS_OK;
}

static LoomSceneTrigger trig[14];
static const loom_u16 hooks[12] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 999, 20, 21};
static LoomMovementScene mv;
static LoomSceneRecord rec;
static LoomInputSnapshot input;

static void set(loom_u8 i, loom_s16 x, loom_s16 y, loom_u16 w, loom_u16 h, loom_u8 flags,
                loom_u8 action, loom_u8 first, loom_u8 count, loom_u8 sprite) {
  trig[i].x = x; trig[i].y = y; trig[i].width = w; trig[i].height = h;
  trig[i].flags = flags; trig[i].adventure_action = action;
  trig[i].first_hook = first; trig[i].hook_count = count;
  trig[i].sprite_slot = sprite; trig[i].adventure_flag = i;
  trig[i].target_scene = (loom_u16)(i % 4u == 3u ? 0xffffu : i % 3u);
  trig[i].target_spawn_x = (loom_s16)(i * 10); trig[i].target_spawn_y = (loom_s16)(-i);
  trig[i].adventure_gate_flag = (loom_u8)(i == 5u ? 7u : 0u);
}

static void tick(loom_s16 x, loom_s16 y) {
  LoomStatus st;
  fake_x = x; fake_y = y;
  printf("%d,%d:", x, y);
  st = loom_scene_check_triggers(&input);
  printf(" =%u in%x once%x taken%x idle%u ap%u d%u c%u ph%u\n", (unsigned)st,
         loom_scene_state.inside_mask, loom_scene_state.once_mask, loom_scene_state.taken_mask,
         (unsigned)loom_scene_state.idle_valid, applied, loom_scene_state.trigger_dispatch_count,
         loom_scene_state.collect_count, (unsigned)loom_scene_state.transition_phase);
}

int main(void) {
  loom_s16 x;
  mv.collider_x = 2; mv.collider_y = -12; mv.collider_width = 12; mv.collider_height = 12;
  set(0, 0, 0, 32, 32, 0, LOOM_ADVENTURE_ACTION_NONE, 0, 2, 0xffu);
  set(1, 24, 0, 16, 16, LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY, LOOM_ADVENTURE_ACTION_NONE, 2, 1, 4);
  set(2, 48, -8, 8, 40, 0, LOOM_ADVENTURE_ACTION_COLLECT, 3, 1, 9);
  set(3, 60, 0, 30, 8, 0, LOOM_ADVENTURE_ACTION_INTERACTION, 4, 2, 0xffu);
  set(4, 100, -20, 4, 4, 0, LOOM_ADVENTURE_ACTION_PICKUP, 0, 0, 0xffu);
  set(5, 90, 0, 20, 20, 0, LOOM_ADVENTURE_ACTION_NONE, 6, 1, 0xffu);
  set(6, 120, 0, 10, 10, LOOM_SCENE_TRIGGER_ONCE_PER_ENTRY, LOOM_ADVENTURE_ACTION_COLLECT, 7, 1, 12);
  set(7, 140, -4, 16, 16, 0, LOOM_ADVENTURE_ACTION_NONE, 11, 2, 0xffu);
  set(8, 160, 0, 8, 8, 0, LOOM_ADVENTURE_ACTION_NONE, 10, 1, 0xffu);
  set(9, 200, 0, 8, 8, LOOM_SCENE_TRIGGER_EXIT, LOOM_ADVENTURE_ACTION_NONE, 8, 1, 0xffu);
  set(10, 220, 0, 8, 8, 0, LOOM_ADVENTURE_ACTION_NONE, 9, 1, 0xffu);
  set(11, 240, 0, 8, 8, LOOM_SCENE_TRIGGER_EXIT, LOOM_ADVENTURE_ACTION_NONE, 0, 0, 0xffu);
  set(12, -40, -40, 400, 4, 0, LOOM_ADVENTURE_ACTION_NONE, 0, 0, 0xffu);
  set(13, 250, 0, 8, 8, LOOM_SCENE_TRIGGER_EXIT, LOOM_ADVENTURE_ACTION_NONE, 0, 0, 0xffu);
  rec.movement = &mv; rec.triggers = trig; rec.hook_ids = hooks;
  rec.trigger_count = 14; rec.hook_count = 12;
  loom_scene_state.record = &rec;
  loom_scene_state.visibility_pending = LOOM_TRUE;
  loom_scene_state.transition_phase = 0;
  for (x = -20; x <= 170; x += 10) {
    tick(x, 10);
    if (x == 20) tick(x, 10);
    if (x == 40) { tick(x, 10); fake_epoch = 1; tick(x, 10); }
    if (x == 70) { input.pads[0].held = 0x80u; tick(x, 10); input.pads[0].held = 0; }
  }
  tick(-60, -20);
  tick(-60, -20);
  loom_scene_state.visibility_pending = LOOM_TRUE;
  tick(-60, -20);
  tick(160, 10);
  tick(200, 10);
  loom_scene_state.transition_phase = 0;
  tick(220, 10);
  tick(240, 10);
  tick(250, 10);
  trig[4].width = 0; fake_epoch = 2;
  tick(0, 0);
  trig[4].width = 4; trig[4].flags = 0x40u;
  tick(1, 0);
  return 0;
}
