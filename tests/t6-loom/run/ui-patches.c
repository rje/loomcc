// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: ui-patches.expected-output
// loomcc-asm-sources: ui-patches.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/ui.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the project UI's per-patch decisions: every predicate over
// in-range and out-of-range bindings, a patch's variant (visibility, value
// lookup, and the meter's multiply-free fill count), the allowed-value
// scan, the range check and the changed-values scan, compared with the
// 816-tcc build. The driver supplies the generated catalog.
#include "../loom-d88b68b/runtime/src/ui.c"
int printf(const char *fmt, ...);
static loom_u16 target[6];
static loom_u16 current[6];
static loom_u16 presented[6];
const LoomUiProjectCatalog loom_generated_project_ui_catalog = {
  .target_values = target, .current_values = current, .presented_values = presented, .binding_count = 6 };
static const loom_u16 variants[5] = { 3, 9, 27, 81, 3 };
static const loom_u16 samples[8] = { 0, 1, 2, 3, 27, 81, 1000, 65535u };
int main(void) {
  LoomUiProjectPatch patch;
  LoomUiBindingRecord binding;
  loom_u8 p, s, b;
  loom_u16 v, m;
  for (s = 0; s < 8; s++) {
    target[2] = samples[s];
    printf("%u:", samples[s]);
    for (p = 0; p < 7; p++) {
      patch.predicate = p == 6 ? LOOM_UI_PREDICATE_NONE : p;
      patch.predicate_value = 27;
      for (b = 2; b <= 7; b += 5) {
        patch.predicate_binding_index = b;
        printf("%u", (unsigned)loom_project_ui_predicate(&patch));
      }
      printf(p == 6 ? " " : ".");
    }
    patch.kind = LOOM_UI_PATCH_VISIBILITY;
    patch.predicate = LOOM_UI_PREDICATE_AT_LEAST;
    patch.predicate_binding_index = 2;
    printf("vis%u ", loom_project_ui_patch_variant(&patch));
    patch.kind = LOOM_UI_PATCH_ICON;
    patch.binding_index = 2;
    patch.variant_values = variants;
    patch.variant_count = 5;
    printf("var%u ", loom_project_ui_patch_variant(&patch));
    patch.variant_count = 0;
    printf("%u\n", loom_project_ui_patch_variant(&patch));
  }
  patch.kind = LOOM_UI_PATCH_METER;
  patch.binding_index = 1;
  patch.secondary_binding_index = 4;
  for (m = 0; m < 5; m++) {
    static const loom_u16 maxima[5] = { 0, 1, 7, 100, 65535u };
    target[4] = maxima[m];
    printf("max %u:", maxima[m]);
    for (v = 0; v < 8; v++) {
      static const loom_u16 vals[8] = { 0, 1, 3, 6, 7, 50, 99, 65535u };
      target[1] = vals[v];
      patch.row_words = (loom_u8)(v + 1);
      patch.row_count = (loom_u8)(v & 1 ? 2 : 1);
      printf(" %u", loom_project_ui_patch_variant(&patch));
    }
    printf("\n");
  }
  binding.allowed_values = variants;
  for (binding.allowed_count = 0; binding.allowed_count <= 5; binding.allowed_count += 1) {
    printf("allowed %u:", binding.allowed_count);
    for (s = 0; s < 8; s++) printf("%u", (unsigned)loom_project_ui_allowed(&binding, samples[s]));
    printf("\n");
  }
  for (s = 0; s < 8; s++)
    for (b = 0; b < 8; b++)
      printf("%u%s", (unsigned)loom_project_ui_range_valid(samples[s], samples[b], 1000), b == 7 ? "\n" : "");
  printf("differ %u", (unsigned)loom_project_ui_values_differ());
  current[5] = 1;
  printf(" %u", (unsigned)loom_project_ui_values_differ());
  presented[5] = 1;
  printf(" %u", (unsigned)loom_project_ui_values_differ());
  loom_project_ui_copy_values(current, target);
  printf(" %u", (unsigned)loom_project_ui_values_differ());
  loom_project_ui_copy_values(presented, current);
  printf(" %u\n", (unsigned)loom_project_ui_values_differ());
  return 0;
}
