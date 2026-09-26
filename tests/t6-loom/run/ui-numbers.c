// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: ui-numbers.expected-output
// loomcc-asm-sources: ui-numbers.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/ui.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the project UI's number formatting (the power table, a digit
// of a decimal or hexadecimal patch, decimal composition by subtraction)
// over values, digit counts and padding, compared with the 816-tcc build.
// The driver supplies the generated catalog: only target_values is used.
#include "../loom-d88b68b/runtime/src/ui.c"
int printf(const char *fmt, ...);
static loom_u16 values[4];
const LoomUiProjectCatalog loom_generated_project_ui_catalog = { .target_values = values };
int main(void) {
  static const loom_u16 samples[] = { 0, 7, 42, 999, 1000, 12345, 65535u };
  LoomUiProjectPatch patch;
  loom_u16 out[5];
  loom_u8 s, digits, pad, pos, i;
  for (i = 0; i < 6; i++) printf("pow %u: %u %u\n", (unsigned)i, loom_project_ui_power(10, i), loom_project_ui_power(16, i));
  for (i = 0; i < 16; i++) { loom_ui_digit_zero[i] = (loom_u16)(0x100 + i * 16); loom_ui_digit_blank[i] = 0x7f; }
  for (s = 0; s < 7; s++) {
    values[1] = samples[s];
    printf("%u:", samples[s]);
    for (digits = 1; digits <= 5; digits += 2)
      for (pad = 0; pad < 2; pad++) {
        patch.binding_index = 1; patch.digits = digits; patch.padding = pad ? LOOM_UI_PADDING_ZERO : 0;
        patch.kind = LOOM_UI_PATCH_NUMBER_DECIMAL;
        loom_project_ui_compose_decimal(&patch, 3, out);
        printf(" [");
        for (pos = 0; pos < digits; pos++) printf("%x%s", out[pos], pos + 1 < digits ? "," : "");
        printf("]");
        patch.kind = LOOM_UI_PATCH_NUMBER_HEXADECIMAL;
        printf("h%u", loom_project_ui_number_digit(&patch, (loom_u16)(digits - 1)));
      }
    printf("\n");
  }
  return 0;
}
