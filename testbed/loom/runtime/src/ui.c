#include <loom/generated/runtime_config.h>
#include <loom/ui.h>

#if LOOM_GENERATED_PROJECT_UI_ENABLED
#include <loom/generated/ui.h>
#endif

#define LOOM_UI_DMA_CHUNK_BYTES ((loom_u16)32u)
/* UI loads and patches keep the v0 pacing of three 32-byte jobs per commit. */
#define LOOM_UI_JOBS_PER_COMMIT ((loom_u8)3u)
#define LOOM_UI_BYTES_PER_COMMIT ((loom_u16)96u)
#define LOOM_UI_LOAD_FONT ((loom_u8)0u)
#define LOOM_UI_LOAD_PALETTE ((loom_u8)1u)
#define LOOM_UI_LOAD_MAP ((loom_u8)2u)
#define LOOM_UI_LOAD_COMPLETE ((loom_u8)3u)

typedef struct LoomUiState {
    LoomCommitId pending_commit_id;
    loom_u16 active_screen;
    loom_u16 resident_scene;
    loom_u16 loading_scene;
    loom_u16 loading_screen;
    loom_u16 load_offset;
    loom_u16 next_job_id;
    loom_u8 load_segment;
    loom_u8 pending_load;
    loom_u8 common_ready;
    loom_u8 ready;
    loom_u8 initial_dismissed;
    loom_u8 initialized;
} LoomUiState;

static LoomUiState loom_ui_state;

#if LOOM_GENERATED_PROJECT_UI_ENABLED
/* Composed number runs, one per job of the open commit; the commit reads
 * them from here in VBlank, and the next refresh waits for it. */
#define LOOM_UI_BLOCK_WORDS ((loom_u16)(3u * 5u))
static loom_u16 loom_ui_block_words[LOOM_UI_BLOCK_WORDS];

const loom_u8 *loom_ui_block(loom_u16 *bytes)
{
    if (bytes != (loom_u16 *)0) {
        *bytes = (loom_u16)(LOOM_UI_BLOCK_WORDS * 2u);
    }
    return (const loom_u8 *)loom_ui_block_words;
}
#endif


#if LOOM_GENERATED_PROJECT_UI_ENABLED

typedef struct LoomProjectUiStackEntry {
    loom_u16 view_index;
    loom_u16 dialogue_index;
    loom_u16 page_index;
    loom_u8 focus_index;
    loom_u8 map_slot;
} LoomProjectUiStackEntry;

typedef struct LoomProjectUiState {
    LoomCommitId pending_commit_id;
    loom_u16 bundle_index;
    loom_u16 view_index;
    loom_u16 load_index;
    loom_u16 load_offset;
    loom_u16 pending_load_index;
    loom_u16 pending_load_offset;
    loom_u16 patch_index;
    loom_u16 patch_segment;
    loom_u16 pending_patch_index;
    loom_u16 pending_patch_segment;
    loom_u16 page_load_offset;
    loom_u16 focus_old_index;
    loom_u16 focus_new_index;
    loom_u16 next_job_id;
    LoomUiCommandHandle commands[LOOM_UI_COMMAND_QUEUE_CAPACITY];
    LoomProjectUiStackEntry stack[LOOM_UI_STACK_CAPACITY];
    LoomUiTelemetry telemetry;
    loom_u8 stack_depth;
    loom_u8 command_head;
    loom_u8 command_count;
    loom_u8 presented_map_slot;
    /* Set when a binding is written to something the screen is not showing,
     * so the per-tick check is a flag test rather than a scan of every
     * binding. Cleared when a refresh presents the values it read. */
    loom_u8 values_dirty;
    loom_u8 page_load_slot;
    loom_u8 pending_page_load;
    loom_u8 page_load_active;
    loom_u8 pending_focus;
    loom_u8 focus_active;
    loom_u8 pending_load;
    loom_u8 pending_patch;
    loom_u8 refresh_active;
    /* Bumped by every stack change; loom_ui_blocks_gameplay recomputes only
     * when it differs from blocks_generation. */
    loom_u8 stack_generation;
    loom_u8 blocks_generation;
    loom_u8 blocks_cached;
    loom_u8 ready;
    loom_u8 initialized;
} LoomProjectUiState;

static LoomProjectUiState loom_project_ui_state;

static loom_u16 loom_project_ui_find_bundle(LoomUiBundleHandle handle)
{
    loom_u16 index;

    for (index = 0u;
         index < loom_generated_project_ui_catalog.bundle_count;
         ++index) {
        if (loom_generated_project_ui_catalog.bundles[index].handle ==
            handle) {
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static loom_u16 loom_project_ui_find_view(LoomUiViewHandle handle,
                                           const LoomUiProjectBundle *bundle)
{
    loom_u16 offset;

    for (offset = 0u; offset < bundle->view_count; ++offset) {
        loom_u16 index;

        index = (loom_u16)(bundle->first_view + offset);
        if (loom_generated_project_ui_catalog.views[index].handle ==
            handle) {
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static LoomProjectUiStackEntry *loom_project_ui_top(void)
{
    if (loom_project_ui_state.stack_depth == 0u) {
        return (LoomProjectUiStackEntry *)0;
    }
    return &loom_project_ui_state.stack[
        loom_project_ui_state.stack_depth - 1u];
}

static loom_u8 loom_project_ui_command_valid(
    LoomUiCommandHandle command)
{
    loom_u16 index;

    for (index = 0u;
         index < loom_generated_project_ui_catalog.command_count;
         ++index) {
        if (loom_generated_project_ui_catalog.commands[index] == command) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

static LoomStatus loom_project_ui_enqueue(
    LoomUiCommandHandle command)
{
    loom_u8 tail;

    if (loom_project_ui_command_valid(command) == LOOM_FALSE) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (loom_project_ui_state.command_count >=
        LOOM_UI_COMMAND_QUEUE_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    tail = (loom_u8)((loom_project_ui_state.command_head +
                      loom_project_ui_state.command_count) %
                     LOOM_UI_COMMAND_QUEUE_CAPACITY);
    loom_project_ui_state.commands[tail] = command;
    ++loom_project_ui_state.command_count;
    return LOOM_STATUS_OK;
}

static loom_u16 loom_project_ui_find_dialogue(
    LoomUiDialogueHandle handle,
    const LoomUiProjectBundle *bundle)
{
    loom_u16 offset;

    for (offset = 0u; offset < bundle->dialogue_count; ++offset) {
        loom_u16 index;

        index = (loom_u16)(bundle->first_dialogue + offset);
        if (loom_generated_project_ui_catalog.dialogues[index].handle ==
            handle) {
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static loom_u8 loom_project_ui_busy(void)
{
    return loom_project_ui_state.pending_page_load != LOOM_FALSE ||
                   loom_project_ui_state.page_load_active != LOOM_FALSE ||
                   loom_project_ui_state.pending_focus != LOOM_FALSE ||
                   loom_project_ui_state.focus_active != LOOM_FALSE ||
                   loom_project_ui_state.pending_patch != LOOM_FALSE ||
                   loom_project_ui_state.refresh_active != LOOM_FALSE
               ? LOOM_TRUE
               : LOOM_FALSE;
}

/* Game code tends to write the same binding every tick, so the last lookup
 * is remembered: the handle scan costs 816-tcc a scanline per record. */
static LoomUiBindingHandle loom_project_ui_last_binding_handle;
static loom_u16 loom_project_ui_last_binding_index;

static loom_u16 loom_project_ui_find_binding(
    LoomUiBindingHandle handle)
{
    loom_u16 index;

    if (loom_project_ui_last_binding_index != LOOM_UI_SCREEN_NONE &&
        loom_project_ui_last_binding_handle == handle) {
        return loom_project_ui_last_binding_index;
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.binding_count;
         ++index) {
        if (loom_generated_project_ui_catalog.bindings[index].handle ==
            handle) {
            loom_project_ui_last_binding_handle = handle;
            loom_project_ui_last_binding_index = index;
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static loom_u8 loom_project_ui_allowed(
    const LoomUiBindingRecord *binding,
    loom_u16 value)
{
    loom_u16 index;

    for (index = 0u; index < binding->allowed_count; ++index) {
        if (binding->allowed_values[index] == value) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

static LoomStatus loom_project_ui_set(LoomUiBindingHandle handle,
                                      loom_u8 expected_type,
                                      loom_u16 value)
{
    const LoomUiBindingRecord *binding;
    loom_u16 index;

    if (loom_project_ui_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_project_ui_find_binding(handle);
    if (index == LOOM_UI_SCREEN_NONE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    binding = &loom_generated_project_ui_catalog.bindings[index];
    if (binding->type != expected_type || binding->reserved != 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if ((expected_type == LOOM_UI_BINDING_BOOL && value > 1u) ||
        (expected_type == LOOM_UI_BINDING_ENUM &&
         value >= binding->allowed_count) ||
        ((expected_type == LOOM_UI_BINDING_ICON ||
          expected_type == LOOM_UI_BINDING_MESSAGE) &&
         loom_project_ui_allowed(binding, value) == LOOM_FALSE)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_generated_project_ui_catalog.current_values[index] = value;
    if (value != loom_generated_project_ui_catalog.presented_values[index]) {
        loom_project_ui_state.values_dirty = LOOM_TRUE;
    }
    return LOOM_STATUS_OK;
}

static loom_u8 loom_project_ui_predicate(
    const LoomUiProjectPatch *patch)
{
    loom_u16 value;

    if (patch->predicate == LOOM_UI_PREDICATE_NONE) {
        return LOOM_TRUE;
    }
    if (patch->predicate_binding_index >=
        loom_generated_project_ui_catalog.binding_count) {
        return LOOM_FALSE;
    }
    value = loom_generated_project_ui_catalog.target_values[
        patch->predicate_binding_index];
    if (patch->predicate == LOOM_UI_PREDICATE_IS_TRUE) {
        return value != 0u ? LOOM_TRUE : LOOM_FALSE;
    }
    if (patch->predicate == LOOM_UI_PREDICATE_IS_FALSE) {
        return value == 0u ? LOOM_TRUE : LOOM_FALSE;
    }
    if (patch->predicate == LOOM_UI_PREDICATE_EQUAL) {
        return value == patch->predicate_value ? LOOM_TRUE : LOOM_FALSE;
    }
    if (patch->predicate == LOOM_UI_PREDICATE_NOT_EQUAL) {
        return value != patch->predicate_value ? LOOM_TRUE : LOOM_FALSE;
    }
    if (patch->predicate == LOOM_UI_PREDICATE_LESS_THAN) {
        return value < patch->predicate_value ? LOOM_TRUE : LOOM_FALSE;
    }
    if (patch->predicate == LOOM_UI_PREDICATE_AT_LEAST) {
        return value >= patch->predicate_value ? LOOM_TRUE : LOOM_FALSE;
    }
    return LOOM_FALSE;
}

static loom_u16 loom_project_ui_patch_variant(
    const LoomUiProjectPatch *patch)
{
    loom_u16 value;
    loom_u16 index;

    if (patch->kind == LOOM_UI_PATCH_VISIBILITY) {
        return loom_project_ui_predicate(patch) != LOOM_FALSE ? 1u : 0u;
    }
    value = loom_generated_project_ui_catalog.target_values[
        patch->binding_index];
    if (patch->kind == LOOM_UI_PATCH_METER) {
        loom_u16 cells;
        loom_u16 filled;
        loom_u16 remainder;
        loom_u16 maximum;

        cells = (loom_u16)((loom_u16)patch->row_words *
                           (loom_u16)patch->row_count);
        maximum = loom_generated_project_ui_catalog.target_values[
            patch->secondary_binding_index];
        if (maximum == 0u) {
            return 0u;
        }
        if (value > maximum) {
            value = maximum;
        }
        /* Compute floor(value * cells / maximum) without requiring a
         * compiler-specific 32-bit cartridge integer type. */
        filled = 0u;
        remainder = 0u;
        while (cells != 0u) {
            if (remainder >= (loom_u16)(maximum - value)) {
                remainder = (loom_u16)(remainder -
                                       (loom_u16)(maximum - value));
                ++filled;
            } else {
                remainder = (loom_u16)(remainder + value);
            }
            --cells;
        }
        return filled;
    }
    for (index = 0u; index < patch->variant_count; ++index) {
        if (patch->variant_values[index] == value) {
            return index;
        }
    }
    return 0u;
}

/* Powers of ten and sixteen that fit sixteen bits, looked up rather than
 * multiplied out (with an overflow divide each step) for every digit. */
static const loom_u16 loom_project_ui_decimal_powers[5] = {1u, 10u, 100u, 1000u, 10000u};

#if LOOM_GENERATED_PROJECT_UI_ENABLED
/* A decimal number's '0' and blank tile words, read from its patch the
 * first time it is drawn. The digits' glyphs are consecutive tiles, so digit
 * d is zero + d; a patch whose variants are not (checked on that first
 * read) keeps the job-per-digit path. */
#define LOOM_UI_DIGIT_CACHE ((loom_u16)16u)
#define LOOM_UI_DIGITS_UNKNOWN ((loom_u8)0u)
#define LOOM_UI_DIGITS_FAST ((loom_u8)1u)
#define LOOM_UI_DIGITS_SLOW ((loom_u8)2u)
static loom_u16 loom_ui_digit_zero[LOOM_UI_DIGIT_CACHE];
static loom_u16 loom_ui_digit_blank[LOOM_UI_DIGIT_CACHE];
static loom_u8 loom_ui_digit_state[LOOM_UI_DIGIT_CACHE];
#endif

static loom_u16 loom_project_ui_power(loom_u16 radix, loom_u8 exponent)
{
    if (radix == 16u) {
        return exponent < 4u ? (loom_u16)(1u << (exponent * 4u)) : 0xffffu;
    }
    return exponent < 5u ? loom_project_ui_decimal_powers[exponent] : 0xffffu;
}

static loom_u16 loom_project_ui_number_digit(
    const LoomUiProjectPatch *patch,
    loom_u16 segment)
{
    loom_u16 value;
    loom_u16 divisor;
    loom_u16 radix;
    loom_u8 exponent;

    value = loom_generated_project_ui_catalog.target_values[
        patch->binding_index];
    radix = patch->kind == LOOM_UI_PATCH_NUMBER_HEXADECIMAL
                ? 16u
                : 10u;
    exponent = (loom_u8)(patch->digits - (loom_u8)segment - 1u);
    divisor = loom_project_ui_power(radix, exponent);
    if (segment + 1u < patch->digits && value < divisor &&
        patch->padding != LOOM_UI_PADDING_ZERO) {
        return radix;
    }
    if (radix == 16u) {
        return (loom_u16)((value / divisor) & 15u);
    }
    return (loom_u16)((value / divisor) % 10u);
}

/* Whether patch_index can be sent whole from the UI block: a decimal
 * number of at most five digits whose variants are ten consecutive digit
 * tiles and a blank. */
static loom_u8 loom_project_ui_digits_fast(const LoomUiProjectBundle *bundle,
                                          const LoomUiProjectPatch *patch,
                                          loom_u16 patch_index)
{
    loom_u16 words[11];
    LoomAssetSpan span;
    loom_u8 digit;

    if (patch->kind != LOOM_UI_PATCH_NUMBER_DECIMAL || patch->digits == 0u ||
        patch->digits > 5u || patch->variant_stride != 2u ||
        patch_index >= LOOM_UI_DIGIT_CACHE) {
        return LOOM_FALSE;
    }
    if (loom_ui_digit_state[patch_index] != LOOM_UI_DIGITS_UNKNOWN) {
        return loom_ui_digit_state[patch_index] == LOOM_UI_DIGITS_FAST
                   ? LOOM_TRUE
                   : LOOM_FALSE;
    }
    loom_ui_digit_state[patch_index] = LOOM_UI_DIGITS_SLOW;
    span.handle = bundle->patch_handle;
    span.offset = patch->source_offset;
    span.length = (loom_u16)sizeof(words);
    if (loom_port_asset_read(&span, (loom_u8 *)words, (loom_u16)sizeof(words)) !=
        LOOM_STATUS_OK) {
        return LOOM_FALSE;
    }
    for (digit = 1u; digit < 10u; ++digit) {
        if (words[digit] != (loom_u16)(words[0] + digit)) {
            return LOOM_FALSE;
        }
    }
    loom_ui_digit_zero[patch_index] = words[0];
    loom_ui_digit_blank[patch_index] = words[10];
    loom_ui_digit_state[patch_index] = LOOM_UI_DIGITS_FAST;
    return LOOM_TRUE;
}

/* The number's tile words into out, most significant first: the digits by
 * subtraction against the power table (816-tcc divides in a loop of calls),
 * leading zeros blank unless the patch pads with zeros, and a value too wide
 * for the digits keeps its low ones as the division did. */
static void loom_project_ui_compose_decimal(const LoomUiProjectPatch *patch,
                                            loom_u16 patch_index,
                                            loom_u16 *out)
{
    loom_u16 value;
    loom_u16 original;
    loom_u16 zero;
    loom_u8 position;
    loom_u8 digits;

    digits = patch->digits;
    value = loom_generated_project_ui_catalog.target_values[
        patch->binding_index];
    if (digits < 5u && value >= loom_project_ui_decimal_powers[digits]) {
        value = (loom_u16)(value % loom_project_ui_decimal_powers[digits]);
    }
    original = loom_generated_project_ui_catalog.target_values[
        patch->binding_index];
    zero = loom_ui_digit_zero[patch_index];
    for (position = 0u; position < digits; ++position) {
        loom_u16 divisor;
        loom_u16 digit;

        divisor = loom_project_ui_decimal_powers[digits - 1u - position];
        if (position + 1u < digits && original < divisor &&
            patch->padding != LOOM_UI_PADDING_ZERO) {
            out[position] = loom_ui_digit_blank[patch_index];
            continue;
        }
        digit = 0u;
        while (value >= divisor) {
            value = (loom_u16)(value - divisor);
            ++digit;
        }
        out[position] = (loom_u16)(zero + digit);
    }
}

static loom_u8 loom_project_ui_values_changed(void)
{
    return loom_project_ui_state.values_dirty;
}

static loom_u8 loom_project_ui_values_differ(void)
{
    loom_u16 index;

    for (index = 0u;
         index < loom_generated_project_ui_catalog.binding_count;
         ++index) {
        if (loom_generated_project_ui_catalog.current_values[index] !=
            loom_generated_project_ui_catalog.presented_values[index]) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

static void loom_project_ui_copy_values(loom_u16 *destination,
                                        const loom_u16 *source)
{
    loom_u16 index;

    for (index = 0u;
         index < loom_generated_project_ui_catalog.binding_count;
         ++index) {
        destination[index] = source[index];
    }
}

static loom_u8 loom_project_ui_range_valid(loom_u16 first,
                                           loom_u16 count,
                                           loom_u16 capacity)
{
    return first <= capacity && count <= (loom_u16)(capacity - first)
               ? LOOM_TRUE
               : LOOM_FALSE;
}

static loom_u8 loom_project_ui_patch_valid(
    const LoomUiProjectPatch *patch)
{
    loom_u16 last_word;

    if (patch->source_handle == LOOM_INVALID_HANDLE ||
        patch->kind > LOOM_UI_PATCH_METER ||
        (patch->predicate > LOOM_UI_PREDICATE_AT_LEAST &&
         patch->predicate != LOOM_UI_PREDICATE_NONE) ||
        patch->padding > LOOM_UI_PADDING_SPACE ||
        patch->reserved_index != 0u || patch->reserved0 != 0u ||
        patch->reserved1 != 0u || patch->binding_index >=
            loom_generated_project_ui_catalog.binding_count ||
        (patch->predicate == LOOM_UI_PREDICATE_NONE &&
         patch->predicate_binding_index != LOOM_UI_INDEX_NONE) ||
        (patch->predicate != LOOM_UI_PREDICATE_NONE &&
         patch->predicate_binding_index >=
             loom_generated_project_ui_catalog.binding_count)) {
        return LOOM_FALSE;
    }
    if (patch->kind == LOOM_UI_PATCH_NUMBER_DECIMAL ||
        patch->kind == LOOM_UI_PATCH_NUMBER_HEXADECIMAL) {
        if (patch->digits == 0u || patch->row_words != 1u ||
            patch->row_count != 1u || patch->variant_stride != 2u ||
            patch->variant_values != (const loom_u16 *)0 ||
            patch->secondary_binding_index != LOOM_UI_INDEX_NONE) {
            return LOOM_FALSE;
        }
        last_word = (loom_u16)(patch->destination_word +
                               (loom_u16)patch->digits - 1u);
    } else {
        if (patch->row_words == 0u || patch->row_words > 32u ||
            patch->row_count == 0u || patch->row_count > 28u ||
            patch->variant_count == 0u ||
            patch->variant_stride !=
                (loom_u16)((loom_u16)patch->row_words *
                           (loom_u16)patch->row_count * 2u) ||
            (patch->variant_values == (const loom_u16 *)0 &&
             (patch->kind == LOOM_UI_PATCH_ICON ||
              patch->kind == LOOM_UI_PATCH_MESSAGE)) ||
            (patch->kind == LOOM_UI_PATCH_METER &&
             patch->secondary_binding_index >=
                 loom_generated_project_ui_catalog.binding_count) ||
            (patch->kind != LOOM_UI_PATCH_METER &&
             patch->secondary_binding_index != LOOM_UI_INDEX_NONE)) {
            return LOOM_FALSE;
        }
        last_word = (loom_u16)(patch->destination_word +
            ((loom_u16)patch->row_count - 1u) * 32u +
            (loom_u16)patch->row_words - 1u);
    }
    return patch->destination_word < 32u * 28u &&
                   last_word < 32u * 28u
               ? LOOM_TRUE
               : LOOM_FALSE;
}

static LoomStatus loom_project_ui_validate_catalog(void)
{
    const LoomUiProjectBundle *bundle;
    loom_u16 index;

    if (loom_generated_project_ui_enabled == LOOM_FALSE ||
        loom_generated_project_ui_catalog.binding_count > 64u ||
        loom_generated_project_ui_catalog.view_count == 0u ||
        loom_generated_project_ui_catalog.bundle_count == 0u ||
        loom_generated_project_ui_catalog.bindings ==
            (const LoomUiBindingRecord *)0 ||
        loom_generated_project_ui_catalog.views ==
            (const LoomUiProjectView *)0 ||
        loom_generated_project_ui_catalog.bundles ==
            (const LoomUiProjectBundle *)0 ||
        (loom_generated_project_ui_catalog.patch_count != 0u &&
         loom_generated_project_ui_catalog.patches ==
             (const LoomUiProjectPatch *)0) ||
        (loom_generated_project_ui_catalog.command_count != 0u &&
         loom_generated_project_ui_catalog.commands ==
             (const LoomUiCommandHandle *)0) ||
        (loom_generated_project_ui_catalog.focus_count != 0u &&
         loom_generated_project_ui_catalog.focus ==
             (const LoomUiProjectFocus *)0) ||
        (loom_generated_project_ui_catalog.input_count != 0u &&
         loom_generated_project_ui_catalog.inputs ==
             (const LoomUiProjectInput *)0) ||
        (loom_generated_project_ui_catalog.dialogue_count != 0u &&
         loom_generated_project_ui_catalog.dialogues ==
             (const LoomUiProjectDialogue *)0) ||
        (loom_generated_project_ui_catalog.dialogue_page_count != 0u &&
         loom_generated_project_ui_catalog.dialogue_pages ==
             (const LoomUiProjectDialoguePage *)0) ||
        (loom_generated_project_ui_catalog.dialogue_choice_count != 0u &&
         loom_generated_project_ui_catalog.dialogue_choices ==
             (const LoomUiProjectDialogueChoice *)0) ||
        loom_generated_project_ui_catalog.current_values ==
            (loom_u16 *)0 ||
        loom_generated_project_ui_catalog.target_values ==
            (loom_u16 *)0 ||
        loom_generated_project_ui_catalog.presented_values ==
            (loom_u16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_project_ui_state.bundle_index = loom_project_ui_find_bundle(
        loom_generated_project_ui_catalog.boot_bundle);
    if (loom_project_ui_state.bundle_index == LOOM_UI_SCREEN_NONE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    bundle = &loom_generated_project_ui_catalog.bundles[
        loom_project_ui_state.bundle_index];
    loom_project_ui_state.view_index = loom_project_ui_find_view(
        loom_generated_project_ui_catalog.boot_view, bundle);
    if (loom_project_ui_state.view_index == LOOM_UI_SCREEN_NONE ||
        bundle->tiles_handle == LOOM_INVALID_HANDLE ||
        bundle->palette_handle == LOOM_INVALID_HANDLE ||
        bundle->tiles_bytes == 0u || bundle->palette_bytes != LOOM_UI_BUNDLE_PALETTE_BYTES ||
        bundle->view_count == 0u ||
        bundle->first_view + bundle->view_count >
            loom_generated_project_ui_catalog.view_count ||
        bundle->first_patch + bundle->patch_count >
            loom_generated_project_ui_catalog.patch_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.binding_count;
         ++index) {
        const LoomUiBindingRecord *binding;

        binding = &loom_generated_project_ui_catalog.bindings[index];
        if (binding->handle == LOOM_UI_HANDLE_NONE ||
            binding->type > LOOM_UI_BINDING_MESSAGE ||
            binding->reserved != 0u ||
            (binding->type == LOOM_UI_BINDING_ENUM &&
             (binding->allowed_count == 0u ||
              binding->default_value >= binding->allowed_count)) ||
            ((binding->type == LOOM_UI_BINDING_ICON ||
              binding->type == LOOM_UI_BINDING_MESSAGE) &&
             (binding->allowed_values == (const loom_u16 *)0 ||
              binding->allowed_count == 0u ||
              loom_project_ui_allowed(binding,
                                      binding->default_value) ==
                  LOOM_FALSE))) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.bundle_count;
         ++index) {
        bundle = &loom_generated_project_ui_catalog.bundles[index];
        if (bundle->handle == LOOM_UI_HANDLE_NONE ||
            bundle->tiles_handle == LOOM_INVALID_HANDLE ||
            bundle->palette_handle == LOOM_INVALID_HANDLE ||
            bundle->tiles_bytes == 0u || bundle->palette_bytes != LOOM_UI_BUNDLE_PALETTE_BYTES ||
            bundle->view_count == 0u ||
            loom_project_ui_range_valid(
                bundle->first_view, bundle->view_count,
                loom_generated_project_ui_catalog.view_count) ==
                LOOM_FALSE ||
            loom_project_ui_range_valid(
                bundle->first_patch, bundle->patch_count,
                loom_generated_project_ui_catalog.patch_count) ==
                LOOM_FALSE ||
            loom_project_ui_range_valid(
                bundle->first_dialogue, bundle->dialogue_count,
                loom_generated_project_ui_catalog.dialogue_count) ==
                LOOM_FALSE ||
            (bundle->dialogue_count != 0u &&
             (bundle->scratch_slot > 3u ||
              bundle->patch_handle == LOOM_INVALID_HANDLE)) ||
            bundle->reserved != 0u ||
            (bundle->patch_count != 0u &&
             bundle->patch_handle == LOOM_INVALID_HANDLE)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.view_count;
         ++index) {
        const LoomUiProjectView *view;

        view = &loom_generated_project_ui_catalog.views[index];
        if (view->handle == LOOM_UI_HANDLE_NONE ||
            view->map_handle == LOOM_INVALID_HANDLE ||
            view->map_bytes != LOOM_UI_MAP_BYTES || view->map_slot > 3u ||
            view->presentation > LOOM_UI_PRESENTATION_REPLACE ||
            loom_project_ui_range_valid(
                view->first_patch, view->patch_count,
                loom_generated_project_ui_catalog.patch_count) ==
                LOOM_FALSE ||
            loom_project_ui_range_valid(
                view->first_focus, view->focus_count,
                loom_generated_project_ui_catalog.focus_count) ==
                LOOM_FALSE ||
            loom_project_ui_range_valid(
                view->first_input, view->input_count,
                loom_generated_project_ui_catalog.input_count) ==
                LOOM_FALSE ||
            (view->focus_count != 0u &&
             view->focus_cursor_source_offset == LOOM_UI_SCREEN_NONE)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.patch_count;
         ++index) {
        if (loom_project_ui_patch_valid(
                &loom_generated_project_ui_catalog.patches[index]) ==
            LOOM_FALSE) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.focus_count;
         ++index) {
        const LoomUiProjectFocus *focus;

        focus = &loom_generated_project_ui_catalog.focus[index];
        if (loom_project_ui_command_valid(focus->command) == LOOM_FALSE ||
            focus->destination_word >= 32u * 28u ||
            focus->blank_source_offset == LOOM_UI_SCREEN_NONE ||
            focus->close_after_command > LOOM_TRUE ||
            focus->reserved != 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.input_count;
         ++index) {
        const LoomUiProjectInput *input;

        input = &loom_generated_project_ui_catalog.inputs[index];
        if (input->button_mask == 0u ||
            (input->button_mask &
             (loom_u16)(input->button_mask - 1u)) != 0u ||
            input->response > LOOM_UI_INPUT_COMMAND ||
            input->close_after_command > LOOM_TRUE ||
            (input->response == LOOM_UI_INPUT_COMMAND &&
             loom_project_ui_command_valid(input->command) ==
                 LOOM_FALSE) ||
            (input->response != LOOM_UI_INPUT_COMMAND &&
             input->command != LOOM_UI_HANDLE_NONE)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.dialogue_count;
         ++index) {
        const LoomUiProjectDialogue *dialogue;

        dialogue = &loom_generated_project_ui_catalog.dialogues[index];
        if (dialogue->handle == LOOM_UI_HANDLE_NONE ||
            dialogue->page_count == 0u ||
            loom_project_ui_range_valid(
                dialogue->first_page, dialogue->page_count,
                loom_generated_project_ui_catalog.dialogue_page_count) ==
                LOOM_FALSE) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.dialogue_page_count;
         ++index) {
        const LoomUiProjectDialoguePage *page;

        page = &loom_generated_project_ui_catalog.dialogue_pages[index];
        if (page->map_handle == LOOM_INVALID_HANDLE ||
            page->message == LOOM_UI_HANDLE_NONE ||
            page->continuation > LOOM_TRUE || page->reserved != 0u ||
            loom_project_ui_range_valid(
                page->first_choice, page->choice_count,
                loom_generated_project_ui_catalog.dialogue_choice_count) ==
                LOOM_FALSE) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.dialogue_choice_count;
         ++index) {
        const LoomUiProjectDialogueChoice *choice;

        choice = &loom_generated_project_ui_catalog.dialogue_choices[index];
        if (loom_project_ui_command_valid(choice->command) == LOOM_FALSE ||
            choice->cursor_destination_word >= 32u * 28u ||
            choice->blank_source_offset == LOOM_UI_SCREEN_NONE ||
            choice->close_after_command > LOOM_TRUE ||
            choice->reserved != 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    return LOOM_STATUS_OK;
}

static void loom_project_ui_current_load(loom_u16 load_index,
                                         LoomAssetHandle *handle,
                                         loom_u16 *destination,
                                         loom_u16 *byte_count,
                                         loom_u8 *destination_kind)
{
    const LoomUiProjectBundle *bundle;

    bundle = &loom_generated_project_ui_catalog.bundles[
        loom_project_ui_state.bundle_index];
    if (load_index == 0u) {
        *handle = bundle->tiles_handle;
        *destination = LOOM_UI_FONT_VRAM_BYTE_ADDRESS;
        *byte_count = bundle->tiles_bytes;
        *destination_kind = LOOM_DMA_DESTINATION_VRAM;
        return;
    }
    if (load_index == 1u) {
        *handle = bundle->palette_handle;
        *destination = LOOM_UI_PALETTE_CGRAM_BYTE_ADDRESS;
        *byte_count = bundle->palette_bytes;
        *destination_kind = LOOM_DMA_DESTINATION_CGRAM;
        return;
    }
    {
        const LoomUiProjectView *view;
        loom_u16 view_index;

        view_index = (loom_u16)(bundle->first_view + load_index - 2u);
        view = &loom_generated_project_ui_catalog.views[view_index];
        *handle = view->map_handle;
        *destination = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
                                  (loom_u16)view->map_slot *
                                      LOOM_UI_MAP_BYTES);
        *byte_count = view->map_bytes;
        *destination_kind = LOOM_DMA_DESTINATION_VRAM;
    }
}

static loom_u16 loom_project_ui_load_count(void)
{
    return (loom_u16)(2u + loom_generated_project_ui_catalog.bundles[
                               loom_project_ui_state.bundle_index]
                               .view_count);
}

static void loom_project_ui_advance_load(void)
{
    loom_project_ui_state.load_index =
        loom_project_ui_state.pending_load_index;
    loom_project_ui_state.load_offset =
        loom_project_ui_state.pending_load_offset;
    if (loom_project_ui_state.load_index >= loom_project_ui_load_count()) {
        loom_project_ui_state.ready = LOOM_TRUE;
    }
}

/* The first load runs on a dark screen, where a transfer that outlasts
 * VBlank loses nothing, so it takes every job and byte the commit has left
 * rather than the patches' three 32-byte jobs: a title with four views is
 * up in a few ticks instead of a hundred. */
static LoomStatus loom_project_ui_add_load_jobs(void)
{
    LoomAssetHandle handle;
    LoomCommitId commit_id;
    LoomDmaJob job;
    LoomStatus status;
    loom_u16 byte_count;
    loom_u16 budget;
    loom_u16 destination;
    loom_u16 load_count;
    loom_u16 load_index;
    loom_u16 offset;
    loom_u16 used;
    loom_u8 destination_kind;
    loom_u8 room;

    (void)loom_frame_build_dma_space(&room);
    used = loom_frame_build_dma_bytes();
    if (room == 0u || used >= LOOM_FRAME_REQUIRED_DMA_BYTES_MAX) {
        return LOOM_STATUS_OK;
    }
    budget = (loom_u16)(LOOM_FRAME_REQUIRED_DMA_BYTES_MAX - used);
    load_count = loom_project_ui_load_count();
    load_index = loom_project_ui_state.load_index;
    offset = loom_project_ui_state.load_offset;
    while (room != 0u && budget != 0u && load_index < load_count) {
        loom_u16 bytes;

        loom_project_ui_current_load(load_index, &handle, &destination,
                                     &byte_count, &destination_kind);
        bytes = (loom_u16)(byte_count - offset);
        if (bytes > budget) {
            bytes = budget;
        }
        job.job_id = loom_project_ui_state.next_job_id;
        job.source_handle = handle;
        job.source_offset = offset;
        job.destination_offset = (loom_u16)(destination + offset);
        job.byte_count = bytes;
        job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
        job.destination_kind = destination_kind;
        job.policy = LOOM_DMA_REQUIRED;
        job.reserved = 0u;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_project_ui_state.next_job_id;
        --room;
        budget = (loom_u16)(budget - bytes);
        offset = (loom_u16)(offset + bytes);
        if (offset == byte_count) {
            ++load_index;
            offset = 0u;
        }
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_project_ui_state.pending_load_index = load_index;
    loom_project_ui_state.pending_load_offset = offset;
    loom_project_ui_state.pending_commit_id = commit_id;
    loom_project_ui_state.pending_load = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

static void loom_project_ui_finish_refresh(void)
{
    loom_project_ui_copy_values(
        loom_generated_project_ui_catalog.presented_values,
        loom_generated_project_ui_catalog.target_values);
    loom_project_ui_state.refresh_active = LOOM_FALSE;
    /* The target was read from current when the refresh began; a write that
     * landed since then set the flag again and is picked up next tick. */
    loom_project_ui_state.values_dirty =
        loom_project_ui_values_differ() != LOOM_FALSE ? LOOM_TRUE : LOOM_FALSE;
}

static void loom_project_ui_begin_refresh(void)
{
    const LoomUiProjectView *view;

    view = &loom_generated_project_ui_catalog.views[
        loom_project_ui_state.view_index];
    loom_project_ui_copy_values(
        loom_generated_project_ui_catalog.target_values,
        loom_generated_project_ui_catalog.current_values);
    loom_project_ui_state.patch_index = view->first_patch;
    loom_project_ui_state.patch_segment = 0u;
    loom_project_ui_state.refresh_active = LOOM_TRUE;
    if (view->patch_count == 0u) {
        loom_project_ui_finish_refresh();
    }
}

static LoomStatus loom_project_ui_add_patch_jobs(void)
{
    const LoomUiProjectBundle *bundle;
    const LoomUiProjectView *view;
    LoomCommitId commit_id;
    LoomDmaJob job;
    LoomStatus status;
    loom_u16 patch_index;
    loom_u16 segment;
    loom_u16 view_patch_end;
    loom_u16 used_bytes;
    loom_u16 block_used;
    loom_u8 count;

    bundle = &loom_generated_project_ui_catalog.bundles[
        loom_project_ui_state.bundle_index];
    view = &loom_generated_project_ui_catalog.views[
        loom_project_ui_state.view_index];
    block_used = 0u;
    patch_index = loom_project_ui_state.patch_index;
    segment = loom_project_ui_state.patch_segment;
    view_patch_end = (loom_u16)(view->first_patch + view->patch_count);
    used_bytes = 0u;
    count = 0u;
    while (patch_index < view_patch_end &&
           count < LOOM_UI_JOBS_PER_COMMIT) {
        const LoomUiProjectPatch *patch;
        loom_u16 bytes;
        loom_u16 destination;
        loom_u16 segment_count;
        loom_u16 source_offset;
        loom_u16 variant;

        patch = &loom_generated_project_ui_catalog.patches[patch_index];
        segment_count =
            patch->kind == LOOM_UI_PATCH_NUMBER_DECIMAL ||
                    patch->kind == LOOM_UI_PATCH_NUMBER_HEXADECIMAL
                ? patch->digits
                : patch->row_count;
        if (patch->kind != LOOM_UI_PATCH_VISIBILITY &&
            loom_project_ui_predicate(patch) == LOOM_FALSE) {
            ++patch_index;
            segment = 0u;
            continue;
        }
        if (segment >= segment_count) {
            ++patch_index;
            segment = 0u;
            continue;
        }
        if (segment == 0u &&
            (loom_u16)(block_used + patch->digits) <= LOOM_UI_BLOCK_WORDS &&
            loom_project_ui_digits_fast(bundle, patch, patch_index) !=
                LOOM_FALSE) {
            bytes = (loom_u16)((loom_u16)patch->digits * 2u);
            if (count != 0u &&
                (loom_u16)(used_bytes + bytes) > LOOM_UI_BYTES_PER_COMMIT) {
                break;
            }
            loom_project_ui_compose_decimal(patch, patch_index,
                                            &loom_ui_block_words[block_used]);
            job.job_id = loom_project_ui_state.next_job_id;
            job.source_handle = LOOM_UI_BLOCK_HANDLE;
            job.source_offset = (loom_u16)(block_used * 2u);
            job.destination_offset = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
                (loom_u16)view->map_slot * LOOM_UI_MAP_BYTES +
                patch->destination_word * 2u);
            job.byte_count = bytes;
            job.source_kind = LOOM_DMA_SOURCE_WRAM_BLOCK;
            job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
            job.policy = LOOM_DMA_REQUIRED;
            job.reserved = 0u;
            status = loom_frame_build_add_dma(&job);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            ++loom_project_ui_state.next_job_id;
            ++count;
            used_bytes = (loom_u16)(used_bytes + bytes);
            block_used = (loom_u16)(block_used + patch->digits);
            ++patch_index;
            segment = 0u;
            continue;
        }
        if (patch->kind == LOOM_UI_PATCH_NUMBER_DECIMAL ||
            patch->kind == LOOM_UI_PATCH_NUMBER_HEXADECIMAL) {
            variant = loom_project_ui_number_digit(patch, segment);
            source_offset = (loom_u16)(patch->source_offset +
                                       variant * patch->variant_stride);
            destination = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
                (loom_u16)view->map_slot * LOOM_UI_MAP_BYTES +
                (loom_u16)(patch->destination_word + segment) * 2u);
            bytes = 2u;
        } else {
            variant = loom_project_ui_patch_variant(patch);
            source_offset = (loom_u16)(patch->source_offset +
                variant * patch->variant_stride +
                segment * (loom_u16)patch->row_words * 2u);
            destination = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
                (loom_u16)view->map_slot * LOOM_UI_MAP_BYTES +
                (patch->destination_word + segment * 32u) * 2u);
            bytes = (loom_u16)patch->row_words * 2u;
        }
        if (count != 0u &&
            (loom_u16)(used_bytes + bytes) >
                LOOM_UI_BYTES_PER_COMMIT) {
            break;
        }
        job.job_id = loom_project_ui_state.next_job_id;
        job.source_handle = bundle->patch_handle;
        job.source_offset = source_offset;
        job.destination_offset = destination;
        job.byte_count = bytes;
        job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
        job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
        job.policy = LOOM_DMA_REQUIRED;
        job.reserved = 0u;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_project_ui_state.next_job_id;
        ++count;
        used_bytes = (loom_u16)(used_bytes + bytes);
        ++segment;
        if (segment == segment_count) {
            ++patch_index;
            segment = 0u;
        }
    }
    if (count == 0u) {
        loom_project_ui_state.patch_index = patch_index;
        loom_project_ui_state.patch_segment = segment;
        if (patch_index == view_patch_end) {
            loom_project_ui_finish_refresh();
        }
        return LOOM_STATUS_OK;
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_project_ui_state.pending_commit_id = commit_id;
    loom_project_ui_state.pending_patch_index = patch_index;
    loom_project_ui_state.pending_patch_segment = segment;
    loom_project_ui_state.pending_patch = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

static void loom_project_ui_observe_boundary(
    const LoomFrameBoundary *boundary)
{
    LoomProjectUiStackEntry *top;

    /* Nearly every tick nothing is in flight: no pending job can be
     * presented, and the telemetry below counts only commits that carried
     * one (a commit id matches pending_commit_id only while it is pending,
     * since ids only grow). */
    if ((loom_u8)(loom_project_ui_state.pending_load |
                  loom_project_ui_state.pending_patch |
                  loom_project_ui_state.pending_page_load |
                  loom_project_ui_state.pending_focus) == 0u) {
        return;
    }
    if (boundary->presentation != LOOM_PRESENTATION_NEW_COMMIT ||
        boundary->presented_commit_id !=
            loom_project_ui_state.pending_commit_id) {
        if ((loom_project_ui_state.pending_load != LOOM_FALSE ||
             loom_project_ui_state.pending_patch != LOOM_FALSE ||
             loom_project_ui_state.pending_page_load != LOOM_FALSE ||
             loom_project_ui_state.pending_focus != LOOM_FALSE) &&
            boundary->presentation ==
                LOOM_PRESENTATION_REPEATED &&
            loom_project_ui_state.telemetry.repeated_boundaries !=
                0xffffu) {
            ++loom_project_ui_state.telemetry.repeated_boundaries;
        }
        return;
    }
    if (loom_project_ui_state.telemetry.accepted_commits != 0xffffu) {
        ++loom_project_ui_state.telemetry.accepted_commits;
    }
    if (loom_project_ui_state.pending_load != LOOM_FALSE) {
        loom_project_ui_advance_load();
        loom_project_ui_state.pending_load = LOOM_FALSE;
    }
    if (loom_project_ui_state.pending_patch != LOOM_FALSE) {
        const LoomUiProjectView *view;

        loom_project_ui_state.patch_index =
            loom_project_ui_state.pending_patch_index;
        loom_project_ui_state.patch_segment =
            loom_project_ui_state.pending_patch_segment;
        loom_project_ui_state.pending_patch = LOOM_FALSE;
        view = &loom_generated_project_ui_catalog.views[
            loom_project_ui_state.view_index];
        if (loom_project_ui_state.patch_index ==
            (loom_u16)(view->first_patch + view->patch_count)) {
            loom_project_ui_finish_refresh();
        }
    }
    if (loom_project_ui_state.pending_page_load != LOOM_FALSE) {
        loom_project_ui_state.page_load_offset =
            (loom_u16)(loom_project_ui_state.page_load_offset +
                       loom_project_ui_state.telemetry.last_commit_bytes);
        loom_project_ui_state.pending_page_load = LOOM_FALSE;
        if (loom_project_ui_state.page_load_offset ==
            LOOM_UI_MAP_BYTES) {
            top = loom_project_ui_top();
            if (top != (LoomProjectUiStackEntry *)0) {
                top->map_slot = loom_project_ui_state.page_load_slot;
                loom_project_ui_state.view_index = top->view_index;
                loom_project_ui_state.presented_map_slot = top->map_slot;
                loom_project_ui_state.telemetry.dialogue_page =
                    top->page_index;
            }
            loom_project_ui_state.page_load_active = LOOM_FALSE;
        }
    }
    if (loom_project_ui_state.pending_focus != LOOM_FALSE) {
        loom_project_ui_state.pending_focus = LOOM_FALSE;
        loom_project_ui_state.focus_active = LOOM_FALSE;
    }
}

static void loom_project_ui_update_telemetry(void)
{
    LoomProjectUiStackEntry *top;

    top = loom_project_ui_top();
    loom_project_ui_state.telemetry.stack_depth =
        loom_project_ui_state.stack_depth;
    loom_project_ui_state.telemetry.focus_index =
        top != (LoomProjectUiStackEntry *)0 ? top->focus_index : 0u;
    loom_project_ui_state.telemetry.commands_queued =
        loom_project_ui_state.command_count;
    loom_project_ui_state.telemetry.active_view =
        top != (LoomProjectUiStackEntry *)0
            ? loom_generated_project_ui_catalog.views[top->view_index]
                  .handle
            : LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.telemetry.transition_pending =
        loom_project_ui_busy();
    loom_project_ui_state.telemetry.presented_map_slot =
        loom_project_ui_state.presented_map_slot;
    loom_project_ui_state.telemetry.blocks_gameplay =
        top != (LoomProjectUiStackEntry *)0 &&
                (top->dialogue_index != LOOM_UI_SCREEN_NONE ||
                 loom_generated_project_ui_catalog.views[top->view_index]
                         .presentation != LOOM_UI_PRESENTATION_OVERLAY)
            ? LOOM_TRUE
            : LOOM_FALSE;
}

static LoomStatus loom_project_ui_start_page(loom_u16 page_index,
                                              loom_u8 target_slot)
{
    LoomProjectUiStackEntry *top;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0 ||
        page_index >=
            loom_generated_project_ui_catalog.dialogue_page_count ||
        target_slot > 3u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    top->page_index = page_index;
    top->focus_index = 0u;
    loom_project_ui_state.page_load_slot = target_slot;
    loom_project_ui_state.page_load_offset = 0u;
    loom_project_ui_state.page_load_active = LOOM_TRUE;
    loom_project_ui_state.pending_page_load = LOOM_FALSE;
    loom_project_ui_state.telemetry.dialogue_page =
        LOOM_UI_SCREEN_NONE;
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_add_page_jobs(void)
{
    const LoomUiProjectDialoguePage *page;
    LoomProjectUiStackEntry *top;
    LoomCommitId commit_id;
    LoomDmaJob job;
    LoomStatus status;
    loom_u16 offset;
    loom_u16 remaining;
    loom_u16 used_bytes;
    loom_u8 count;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0 ||
        top->page_index >=
            loom_generated_project_ui_catalog.dialogue_page_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    page = &loom_generated_project_ui_catalog.dialogue_pages[
        top->page_index];
    offset = loom_project_ui_state.page_load_offset;
    remaining = (loom_u16)(LOOM_UI_MAP_BYTES - offset);
    used_bytes = 0u;
    count = 0u;
    while (remaining != 0u && count < LOOM_UI_JOBS_PER_COMMIT) {
        loom_u16 bytes;

        bytes = remaining < LOOM_UI_DMA_CHUNK_BYTES
                    ? remaining
                    : LOOM_UI_DMA_CHUNK_BYTES;
        job.job_id = loom_project_ui_state.next_job_id;
        job.source_handle = page->map_handle;
        job.source_offset = offset;
        job.destination_offset = (loom_u16)(
            LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
            (loom_u16)loom_project_ui_state.page_load_slot *
                LOOM_UI_MAP_BYTES + offset);
        job.byte_count = bytes;
        job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
        job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
        job.policy = LOOM_DMA_REQUIRED;
        job.reserved = 0u;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_project_ui_state.next_job_id;
        ++count;
        used_bytes = (loom_u16)(used_bytes + bytes);
        offset = (loom_u16)(offset + bytes);
        remaining = (loom_u16)(remaining - bytes);
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_project_ui_state.pending_commit_id = commit_id;
    loom_project_ui_state.pending_page_load = LOOM_TRUE;
    loom_project_ui_state.telemetry.last_commit_jobs = count;
    loom_project_ui_state.telemetry.last_commit_bytes = used_bytes;
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static const LoomUiProjectDialoguePage *
loom_project_ui_active_page(void)
{
    LoomProjectUiStackEntry *top;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0 ||
        top->dialogue_index == LOOM_UI_SCREEN_NONE ||
        top->page_index >=
            loom_generated_project_ui_catalog.dialogue_page_count) {
        return (const LoomUiProjectDialoguePage *)0;
    }
    return &loom_generated_project_ui_catalog.dialogue_pages[
        top->page_index];
}

static const LoomUiProjectFocus *loom_project_ui_focus_record(
    loom_u16 view_index,
    loom_u8 focus_index)
{
    const LoomUiProjectView *view;

    view = &loom_generated_project_ui_catalog.views[view_index];
    if (focus_index >= view->focus_count) {
        return (const LoomUiProjectFocus *)0;
    }
    return &loom_generated_project_ui_catalog.focus[
        view->first_focus + focus_index];
}

static const LoomUiProjectDialogueChoice *
loom_project_ui_choice_record(const LoomUiProjectDialoguePage *page,
                              loom_u8 focus_index)
{
    if (page == (const LoomUiProjectDialoguePage *)0 ||
        focus_index >= page->choice_count) {
        return (const LoomUiProjectDialogueChoice *)0;
    }
    return &loom_generated_project_ui_catalog.dialogue_choices[
        page->first_choice + focus_index];
}

static LoomStatus loom_project_ui_add_focus_jobs(void)
{
    const LoomUiProjectBundle *bundle;
    const LoomUiProjectDialogueChoice *old_choice;
    const LoomUiProjectDialogueChoice *new_choice;
    const LoomUiProjectDialoguePage *page;
    const LoomUiProjectFocus *old_focus;
    const LoomUiProjectFocus *new_focus;
    const LoomUiProjectView *view;
    LoomProjectUiStackEntry *top;
    LoomCommitId commit_id;
    LoomDmaJob job;
    LoomStatus status;
    loom_u16 old_blank;
    loom_u16 old_destination;
    loom_u16 new_destination;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    bundle = &loom_generated_project_ui_catalog.bundles[
        loom_project_ui_state.bundle_index];
    view = &loom_generated_project_ui_catalog.views[top->view_index];
    page = loom_project_ui_active_page();
    if (page != (const LoomUiProjectDialoguePage *)0) {
        old_choice = loom_project_ui_choice_record(
            page, (loom_u8)loom_project_ui_state.focus_old_index);
        new_choice = loom_project_ui_choice_record(
            page, (loom_u8)loom_project_ui_state.focus_new_index);
        if (old_choice == (const LoomUiProjectDialogueChoice *)0 ||
            new_choice == (const LoomUiProjectDialogueChoice *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        old_blank = old_choice->blank_source_offset;
        old_destination = old_choice->cursor_destination_word;
        new_destination = new_choice->cursor_destination_word;
    } else {
        old_focus = loom_project_ui_focus_record(
            top->view_index,
            (loom_u8)loom_project_ui_state.focus_old_index);
        new_focus = loom_project_ui_focus_record(
            top->view_index,
            (loom_u8)loom_project_ui_state.focus_new_index);
        if (old_focus == (const LoomUiProjectFocus *)0 ||
            new_focus == (const LoomUiProjectFocus *)0 ||
            old_focus->destination_word == LOOM_UI_SCREEN_NONE ||
            new_focus->destination_word == LOOM_UI_SCREEN_NONE) {
            loom_project_ui_state.focus_active = LOOM_FALSE;
            return LOOM_STATUS_OK;
        }
        old_blank = old_focus->blank_source_offset;
        old_destination = old_focus->destination_word;
        new_destination = new_focus->destination_word;
    }
    job.job_id = loom_project_ui_state.next_job_id++;
    job.source_handle = bundle->patch_handle;
    job.source_offset = old_blank;
    job.destination_offset = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
        (loom_u16)top->map_slot * LOOM_UI_MAP_BYTES +
        old_destination * 2u);
    job.byte_count = 2u;
    job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
    job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
    job.policy = LOOM_DMA_REQUIRED;
    job.reserved = 0u;
    status = loom_frame_build_add_dma(&job);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    job.job_id = loom_project_ui_state.next_job_id++;
    job.source_offset = view->focus_cursor_source_offset;
    job.destination_offset = (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
        (loom_u16)top->map_slot * LOOM_UI_MAP_BYTES +
        new_destination * 2u);
    status = loom_frame_build_add_dma(&job);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_project_ui_state.pending_commit_id = commit_id;
    loom_project_ui_state.pending_focus = LOOM_TRUE;
    loom_project_ui_state.telemetry.last_commit_jobs = 2u;
    loom_project_ui_state.telemetry.last_commit_bytes = 4u;
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_pop_internal(void)
{
    LoomProjectUiStackEntry *top;

    if (loom_project_ui_state.stack_depth == 0u) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    --loom_project_ui_state.stack_depth;
    ++loom_project_ui_state.stack_generation;
    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0) {
        loom_project_ui_state.view_index = LOOM_UI_SCREEN_NONE;
        loom_project_ui_state.telemetry.dialogue_page =
            LOOM_UI_SCREEN_NONE;
    } else {
        loom_project_ui_state.view_index = top->view_index;
        loom_project_ui_state.presented_map_slot = top->map_slot;
        loom_project_ui_state.telemetry.dialogue_page =
            top->dialogue_index != LOOM_UI_SCREEN_NONE
                ? top->page_index
                : LOOM_UI_SCREEN_NONE;
        /* Bindings may have moved while the popped view covered this one;
         * its map slot still shows the values it was last patched with. */
        loom_project_ui_state.values_dirty = LOOM_TRUE;
    }
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_show_view(
    LoomUiViewHandle handle,
    loom_u8 replace)
{
    const LoomUiProjectBundle *bundle;
    LoomProjectUiStackEntry *entry;
    loom_u16 view_index;

    if (loom_project_ui_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_project_ui_busy() != LOOM_FALSE) {
        return LOOM_STATUS_BUSY;
    }
    bundle = &loom_generated_project_ui_catalog.bundles[
        loom_project_ui_state.bundle_index];
    view_index = loom_project_ui_find_view(handle, bundle);
    if (view_index == LOOM_UI_SCREEN_NONE) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (replace != LOOM_FALSE &&
        loom_project_ui_state.stack_depth != 0u) {
        entry = loom_project_ui_top();
    } else {
        if (loom_project_ui_state.stack_depth >=
            LOOM_UI_STACK_CAPACITY) {
            return LOOM_STATUS_CAPACITY;
        }
        entry = &loom_project_ui_state.stack[
            loom_project_ui_state.stack_depth++];
    }
    ++loom_project_ui_state.stack_generation;
    entry->view_index = view_index;
    entry->dialogue_index = LOOM_UI_SCREEN_NONE;
    entry->page_index = LOOM_UI_SCREEN_NONE;
    entry->focus_index = 0u;
    entry->map_slot = loom_generated_project_ui_catalog.views[
        view_index].map_slot;
    loom_project_ui_state.view_index = view_index;
    loom_project_ui_state.presented_map_slot = entry->map_slot;
    loom_project_ui_state.telemetry.dialogue_page =
        LOOM_UI_SCREEN_NONE;
    /* Every view keeps its own map slot, patched only while it is current:
     * a title shown after a save was written behind it would still hide
     * CONTINUE. Refresh the incoming view against the current values. */
    loom_project_ui_state.values_dirty = LOOM_TRUE;
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_move_focus(loom_u8 next)
{
    const LoomUiProjectDialoguePage *page;
    const LoomUiProjectView *view;
    LoomProjectUiStackEntry *top;
    loom_u16 count;
    loom_u8 old_focus;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0) {
        return LOOM_STATUS_OK;
    }
    page = loom_project_ui_active_page();
    view = &loom_generated_project_ui_catalog.views[top->view_index];
    count = page != (const LoomUiProjectDialoguePage *)0
                ? page->choice_count
                : view->focus_count;
    if (count <= 1u) {
        return LOOM_STATUS_OK;
    }
    old_focus = top->focus_index;
    if (next != LOOM_FALSE) {
        top->focus_index =
            (loom_u8)((top->focus_index + 1u) % count);
    } else if (top->focus_index == 0u) {
        top->focus_index = (loom_u8)(count - 1u);
    } else {
        --top->focus_index;
    }
    loom_project_ui_state.focus_old_index = old_focus;
    loom_project_ui_state.focus_new_index = top->focus_index;
    loom_project_ui_state.focus_active = LOOM_TRUE;
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_activate(void)
{
    const LoomUiProjectBundle *bundle;
    const LoomUiProjectDialogue *dialogue;
    const LoomUiProjectDialogueChoice *choice;
    const LoomUiProjectDialoguePage *page;
    const LoomUiProjectFocus *focus;
    const LoomUiProjectView *view;
    LoomProjectUiStackEntry *top;
    LoomStatus status;
    loom_u8 target_slot;

    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0) {
        return LOOM_STATUS_OK;
    }
    page = loom_project_ui_active_page();
    if (page != (const LoomUiProjectDialoguePage *)0) {
        choice = loom_project_ui_choice_record(page, top->focus_index);
        if (choice != (const LoomUiProjectDialogueChoice *)0) {
            status = loom_project_ui_enqueue(choice->command);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            if (choice->close_after_command != LOOM_FALSE) {
                return loom_project_ui_pop_internal();
            }
            loom_project_ui_update_telemetry();
            return LOOM_STATUS_OK;
        }
        dialogue = &loom_generated_project_ui_catalog.dialogues[
            top->dialogue_index];
        if (top->page_index + 1u <
            dialogue->first_page + dialogue->page_count) {
            bundle = &loom_generated_project_ui_catalog.bundles[
                loom_project_ui_state.bundle_index];
            view = &loom_generated_project_ui_catalog.views[
                top->view_index];
            target_slot = top->map_slot == bundle->scratch_slot
                              ? view->map_slot
                              : bundle->scratch_slot;
            return loom_project_ui_start_page(
                (loom_u16)(top->page_index + 1u), target_slot);
        }
        return loom_project_ui_pop_internal();
    }
    focus = loom_project_ui_focus_record(
        top->view_index, top->focus_index);
    if (focus == (const LoomUiProjectFocus *)0) {
        return LOOM_STATUS_OK;
    }
    status = loom_project_ui_enqueue(focus->command);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if (focus->close_after_command != LOOM_FALSE) {
        return loom_project_ui_pop_internal();
    }
    loom_project_ui_update_telemetry();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_dispatch_input(
    const LoomUiProjectInput *binding)
{
    LoomStatus status;

    if (binding->response == LOOM_UI_INPUT_FOCUS_PREVIOUS) {
        return loom_project_ui_move_focus(LOOM_FALSE);
    }
    if (binding->response == LOOM_UI_INPUT_FOCUS_NEXT) {
        return loom_project_ui_move_focus(LOOM_TRUE);
    }
    if (binding->response == LOOM_UI_INPUT_ACTIVATE) {
        return loom_project_ui_activate();
    }
    if (binding->response == LOOM_UI_INPUT_CANCEL ||
        binding->response == LOOM_UI_INPUT_CLOSE) {
        return loom_project_ui_pop_internal();
    }
    if (binding->response == LOOM_UI_INPUT_COMMAND) {
        status = loom_project_ui_enqueue(binding->command);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        if (binding->close_after_command != LOOM_FALSE) {
            return loom_project_ui_pop_internal();
        }
        loom_project_ui_update_telemetry();
        return LOOM_STATUS_OK;
    }
    return LOOM_STATUS_INVALID_ARGUMENT;
}

static LoomStatus loom_project_ui_process_input(
    const LoomInputSnapshot *input)
{
    const LoomUiProjectView *view;
    LoomProjectUiStackEntry *top;
    loom_u16 index;
    loom_u16 pressed;

    if (loom_project_ui_state.ready == LOOM_FALSE ||
        loom_project_ui_busy() != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    top = loom_project_ui_top();
    if (top == (LoomProjectUiStackEntry *)0) {
        return LOOM_STATUS_OK;
    }
    view = &loom_generated_project_ui_catalog.views[top->view_index];
    pressed = input->pads[0].pressed;
    for (index = 0u; index < view->input_count; ++index) {
        const LoomUiProjectInput *binding;

        binding = &loom_generated_project_ui_catalog.inputs[
            view->first_input + index];
        if ((pressed & binding->button_mask) == 0u) {
            continue;
        }
        if (((binding->button_mask == LOOM_BUTTON_UP ||
              binding->button_mask == LOOM_BUTTON_DOWN) &&
             (pressed & (LOOM_BUTTON_UP | LOOM_BUTTON_DOWN)) ==
                 (LOOM_BUTTON_UP | LOOM_BUTTON_DOWN)) ||
            ((binding->button_mask == LOOM_BUTTON_LEFT ||
              binding->button_mask == LOOM_BUTTON_RIGHT) &&
             (pressed & (LOOM_BUTTON_LEFT | LOOM_BUTTON_RIGHT)) ==
                 (LOOM_BUTTON_LEFT | LOOM_BUTTON_RIGHT))) {
            continue;
        }
        return loom_project_ui_dispatch_input(binding);
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_project_ui_initialize(void)
{
    LoomStatus status;
    loom_u16 index;

    loom_project_ui_last_binding_handle = LOOM_UI_HANDLE_NONE;
    loom_project_ui_last_binding_index = LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.pending_commit_id = LOOM_COMMIT_NONE;
    loom_project_ui_state.bundle_index = LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.view_index = LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.load_index = 0u;
    loom_project_ui_state.load_offset = 0u;
    loom_project_ui_state.patch_index = 0u;
    loom_project_ui_state.patch_segment = 0u;
    loom_project_ui_state.pending_patch_index = 0u;
    loom_project_ui_state.pending_patch_segment = 0u;
    loom_project_ui_state.page_load_offset = 0u;
    loom_project_ui_state.focus_old_index = 0u;
    loom_project_ui_state.focus_new_index = 0u;
    loom_project_ui_state.next_job_id = 0x5000u;
    loom_project_ui_state.stack_depth = 0u;
    loom_project_ui_state.stack_generation = 1u;
    loom_project_ui_state.blocks_generation = 0u;
    loom_project_ui_state.blocks_cached = LOOM_FALSE;
    loom_project_ui_state.command_head = 0u;
    loom_project_ui_state.command_count = 0u;
    loom_project_ui_state.presented_map_slot = 0u;
    loom_project_ui_state.page_load_slot = LOOM_UI_INDEX_NONE;
    loom_project_ui_state.pending_page_load = LOOM_FALSE;
    loom_project_ui_state.page_load_active = LOOM_FALSE;
    loom_project_ui_state.pending_focus = LOOM_FALSE;
    loom_project_ui_state.focus_active = LOOM_FALSE;
    loom_project_ui_state.pending_load = LOOM_FALSE;
    loom_project_ui_state.pending_patch = LOOM_FALSE;
    loom_project_ui_state.refresh_active = LOOM_FALSE;
    loom_project_ui_state.ready = LOOM_FALSE;
    loom_project_ui_state.initialized = LOOM_FALSE;
    status = loom_project_ui_validate_catalog();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    for (index = 0u;
         index < loom_generated_project_ui_catalog.binding_count;
         ++index) {
        loom_u16 value;

        value = loom_generated_project_ui_catalog.bindings[index]
                    .default_value;
        loom_generated_project_ui_catalog.current_values[index] = value;
        loom_generated_project_ui_catalog.target_values[index] = value;
        loom_generated_project_ui_catalog.presented_values[index] = value;
    }
    loom_project_ui_state.values_dirty = LOOM_FALSE;
    loom_project_ui_state.initialized = LOOM_TRUE;
    loom_project_ui_state.stack[0].view_index =
        loom_project_ui_state.view_index;
    loom_project_ui_state.stack[0].dialogue_index =
        LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.stack[0].page_index = LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.stack[0].focus_index = 0u;
    loom_project_ui_state.stack[0].map_slot =
        loom_generated_project_ui_catalog.views[
            loom_project_ui_state.view_index].map_slot;
    loom_project_ui_state.presented_map_slot =
        loom_project_ui_state.stack[0].map_slot;
    loom_project_ui_state.stack_depth = 1u;
    ++loom_project_ui_state.stack_generation;
    loom_project_ui_state.telemetry.accepted_commits = 0u;
    loom_project_ui_state.telemetry.repeated_boundaries = 0u;
    loom_project_ui_state.telemetry.dialogue_page =
        LOOM_UI_SCREEN_NONE;
    loom_project_ui_state.telemetry.last_commit_bytes = 0u;
    loom_project_ui_state.telemetry.active_view =
        loom_generated_project_ui_catalog.views[
            loom_project_ui_state.view_index].handle;
    loom_project_ui_state.telemetry.stack_depth = 1u;
    loom_project_ui_state.telemetry.focus_index = 0u;
    loom_project_ui_state.telemetry.commands_queued = 0u;
    loom_project_ui_state.telemetry.last_commit_jobs = 0u;
    loom_project_ui_state.telemetry.transition_pending = LOOM_FALSE;
    loom_project_ui_state.telemetry.presented_map_slot =
        loom_project_ui_state.presented_map_slot;
    loom_project_ui_state.telemetry.blocks_gameplay =
        loom_generated_project_ui_catalog.views[
                loom_project_ui_state.view_index]
                    .presentation != LOOM_UI_PRESENTATION_OVERLAY
            ? LOOM_TRUE
            : LOOM_FALSE;
    loom_project_ui_state.telemetry.reserved = 0u;
    return LOOM_STATUS_OK;
}

/* The resident view's record, found again only when the view changes: a
 * subscript of the view table is a multiply helper on 816-tcc. */
static loom_u16 loom_project_ui_view_cached = LOOM_UI_SCREEN_NONE;
static const LoomUiProjectView *loom_project_ui_view_record;

static const LoomUiProjectView *loom_project_ui_current_view(void)
{
    if (loom_project_ui_state.view_index != loom_project_ui_view_cached) {
        loom_project_ui_view_cached = loom_project_ui_state.view_index;
        loom_project_ui_view_record =
            &loom_generated_project_ui_catalog.views[
                loom_project_ui_state.view_index];
    }
    return loom_project_ui_view_record;
}

static LoomStatus loom_project_ui_build_frame(
    const LoomFrameBoundary *boundary,
    LoomDisplayState *display)
{
    const LoomUiProjectView *view;
    LoomStatus status;

    loom_project_ui_observe_boundary(boundary);
    if (loom_project_ui_state.ready == LOOM_FALSE) {
        display->brightness = 0u;
        display->main_layers = 0u;
        display->flags |= LOOM_DISPLAY_FORCED_BLANK;
        /* The load's jobs join the commit last: loom_ui_build_load. */
        return LOOM_STATUS_OK;
    }
    if (loom_project_ui_state.page_load_active != LOOM_FALSE) {
        if (loom_project_ui_state.pending_page_load == LOOM_FALSE) {
            status = loom_project_ui_add_page_jobs();
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    } else if (loom_project_ui_state.focus_active != LOOM_FALSE) {
        if (loom_project_ui_state.pending_focus == LOOM_FALSE) {
            status = loom_project_ui_add_focus_jobs();
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    } else if (loom_project_ui_active_page() ==
                   (const LoomUiProjectDialoguePage *)0) {
        if (loom_project_ui_state.refresh_active == LOOM_FALSE &&
            loom_project_ui_values_changed() != LOOM_FALSE) {
            loom_project_ui_begin_refresh();
        }
        if (loom_project_ui_state.refresh_active != LOOM_FALSE &&
            loom_project_ui_state.pending_patch == LOOM_FALSE) {
            status = loom_project_ui_add_patch_jobs();
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
    if (loom_project_ui_state.view_index == LOOM_UI_SCREEN_NONE) {
        return LOOM_STATUS_OK;
    }
    view = loom_project_ui_current_view();
    display->bg_scroll_x[LOOM_UI_SCROLL_INDEX] =
        (loom_project_ui_state.presented_map_slot & 1u) != 0u
            ? 256
            : 0;
    display->bg_scroll_y[LOOM_UI_SCROLL_INDEX] =
        (loom_project_ui_state.presented_map_slot & 2u) != 0u
            ? 256
            : 0;
    if (view->presentation == LOOM_UI_PRESENTATION_REPLACE) {
        display->backdrop_color = LOOM_UI_PANEL_COLOR;
        display->main_layers = LOOM_UI_LAYER;
        display->flags |= LOOM_UI_DISPLAY_FLAGS;
        display->color_math_layers = 0u;
        display->color_math_flags = 0u;
    } else {
        display->main_layers |= LOOM_UI_LAYER;
        display->flags |= LOOM_UI_DISPLAY_FLAGS;
    }
    return LOOM_STATUS_OK;
}

#endif

static const LoomUiScreen *loom_ui_screen(loom_u16 index)
{
    if (index == LOOM_UI_SCREEN_NONE ||
        index >= loom_generated_ui_catalog.screen_count) {
        return (const LoomUiScreen *)0;
    }
    return &loom_generated_ui_catalog.screens[index];
}

static LoomStatus loom_ui_validate_catalog(void)
{
    loom_u16 index;

    if (loom_generated_ui_catalog.font_tiles_handle ==
            LOOM_INVALID_HANDLE ||
        loom_generated_ui_catalog.palette_handle ==
            LOOM_INVALID_HANDLE ||
        loom_generated_ui_catalog.font_tiles_bytes == 0u ||
        loom_generated_ui_catalog.palette_bytes == 0u ||
        loom_generated_ui_catalog.screen_count == 0u ||
        loom_generated_ui_catalog.screens ==
            (const LoomUiScreen *)0 ||
        (loom_generated_ui_catalog.initial_screen !=
             LOOM_UI_SCREEN_NONE &&
         loom_generated_ui_catalog.initial_screen >=
             loom_generated_ui_catalog.screen_count)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u;
         index < loom_generated_ui_catalog.screen_count;
         ++index) {
        const LoomUiScreen *screen;

        screen = &loom_generated_ui_catalog.screens[index];
        if (screen->map_handle == LOOM_INVALID_HANDLE ||
            screen->map_bytes == 0u ||
            screen->map_bytes > LOOM_UI_MAP_BYTES ||
            (screen->map_bytes & 1u) != 0u ||
            screen->map_slot > 3u ||
            (screen->required_flags & screen->forbidden_flags) != 0u ||
            (screen->flags &
             (loom_u8)(~(LOOM_UI_SCREEN_INITIAL |
                         LOOM_UI_SCREEN_BLOCKS_GAMEPLAY))) != 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    return LOOM_STATUS_OK;
}

static void loom_ui_select(loom_u16 index)
{
    loom_ui_state.active_screen = index;
}

static loom_u16 loom_ui_first_screen(loom_u16 scene_index)
{
    loom_u16 index;

    for (index = 0u; index < loom_generated_ui_catalog.screen_count;
         ++index) {
        if (loom_generated_ui_catalog.screens[index].scene_index ==
            scene_index) {
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static loom_u16 loom_ui_next_screen(loom_u16 scene_index,
                                    loom_u16 current)
{
    loom_u16 index;

    for (index = (loom_u16)(current + 1u);
         index < loom_generated_ui_catalog.screen_count; ++index) {
        if (loom_generated_ui_catalog.screens[index].scene_index ==
            scene_index) {
            return index;
        }
    }
    return LOOM_UI_SCREEN_NONE;
}

static void loom_ui_start_scene_load(loom_u16 scene_index)
{
    loom_ui_state.loading_scene = scene_index;
    loom_ui_state.loading_screen = loom_ui_first_screen(scene_index);
    loom_ui_state.pending_load = LOOM_FALSE;
    loom_ui_state.pending_commit_id = LOOM_COMMIT_NONE;
    loom_ui_state.load_offset = 0u;
    loom_ui_state.ready = LOOM_FALSE;
    if (loom_ui_state.loading_screen == LOOM_UI_SCREEN_NONE) {
        loom_ui_state.resident_scene = scene_index;
        loom_ui_state.ready = LOOM_TRUE;
        loom_ui_state.load_segment = LOOM_UI_LOAD_COMPLETE;
    } else if (loom_ui_state.common_ready != LOOM_FALSE) {
        loom_ui_state.load_segment = LOOM_UI_LOAD_MAP;
    } else {
        loom_ui_state.load_segment = LOOM_UI_LOAD_FONT;
    }
}

static loom_u16 loom_ui_matching_screen(loom_u16 scene_index,
                                        loom_u16 adventure_flags)
{
    loom_u16 index;

    for (index = 0u;
         index < loom_generated_ui_catalog.screen_count;
         ++index) {
        const LoomUiScreen *screen;

        screen = &loom_generated_ui_catalog.screens[index];
        if ((screen->flags & LOOM_UI_SCREEN_INITIAL) != 0u ||
            screen->scene_index != scene_index ||
            (adventure_flags & screen->required_flags) !=
                screen->required_flags ||
            (adventure_flags & screen->forbidden_flags) != 0u) {
            continue;
        }
        return index;
    }
    return LOOM_UI_SCREEN_NONE;
}

static void loom_ui_current_segment(LoomAssetHandle *handle,
                                    loom_u16 *destination,
                                    loom_u16 *byte_count,
                                    loom_u8 *destination_kind)
{
    const LoomUiScreen *screen;

    if (loom_ui_state.load_segment == LOOM_UI_LOAD_FONT) {
        *handle = loom_generated_ui_catalog.font_tiles_handle;
        *destination = LOOM_UI_FONT_VRAM_BYTE_ADDRESS;
        *byte_count = loom_generated_ui_catalog.font_tiles_bytes;
        *destination_kind = LOOM_DMA_DESTINATION_VRAM;
        return;
    }
    if (loom_ui_state.load_segment == LOOM_UI_LOAD_PALETTE) {
        *handle = loom_generated_ui_catalog.palette_handle;
        *destination = LOOM_UI_PALETTE_CGRAM_BYTE_ADDRESS;
        *byte_count = loom_generated_ui_catalog.palette_bytes;
        *destination_kind = LOOM_DMA_DESTINATION_CGRAM;
        return;
    }
    screen = loom_ui_screen(loom_ui_state.loading_screen);
    *handle = screen != (const LoomUiScreen *)0
                  ? screen->map_handle
                  : LOOM_INVALID_HANDLE;
    *destination = screen != (const LoomUiScreen *)0
                       ? (loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS +
                                    (loom_u16)screen->map_slot *
                                        LOOM_UI_MAP_BYTES)
                       : LOOM_UI_MAP_VRAM_BYTE_ADDRESS;
    *byte_count = screen != (const LoomUiScreen *)0
                      ? screen->map_bytes
                      : 0u;
    *destination_kind = LOOM_DMA_DESTINATION_VRAM;
}

static void loom_ui_advance_load(void)
{
    LoomAssetHandle handle;
    loom_u16 destination;
    loom_u16 byte_count;
    loom_u16 remaining;
    loom_u16 transferred;
    loom_u8 destination_kind;

    loom_ui_current_segment(&handle, &destination, &byte_count,
                            &destination_kind);
    (void)handle;
    (void)destination;
    (void)destination_kind;
    remaining = (loom_u16)(byte_count - loom_ui_state.load_offset);
    transferred = remaining < LOOM_UI_BYTES_PER_COMMIT
                      ? remaining
                      : LOOM_UI_BYTES_PER_COMMIT;
    loom_ui_state.load_offset =
        (loom_u16)(loom_ui_state.load_offset + transferred);
    if (loom_ui_state.load_offset != byte_count) {
        return;
    }
    loom_ui_state.load_offset = 0u;
    if (loom_ui_state.load_segment == LOOM_UI_LOAD_FONT) {
        loom_ui_state.load_segment = LOOM_UI_LOAD_PALETTE;
        return;
    }
    if (loom_ui_state.load_segment == LOOM_UI_LOAD_PALETTE) {
        loom_ui_state.common_ready = LOOM_TRUE;
        loom_ui_state.load_segment = LOOM_UI_LOAD_MAP;
        return;
    }
    loom_ui_state.loading_screen = loom_ui_next_screen(
        loom_ui_state.loading_scene, loom_ui_state.loading_screen);
    if (loom_ui_state.loading_screen != LOOM_UI_SCREEN_NONE) {
        return;
    }
    loom_ui_state.load_segment = LOOM_UI_LOAD_COMPLETE;
    loom_ui_state.resident_scene = loom_ui_state.loading_scene;
    loom_ui_state.ready = LOOM_TRUE;
}

static void loom_ui_observe_boundary(const LoomFrameBoundary *boundary)
{
    if (loom_ui_state.pending_load != LOOM_FALSE &&
        boundary->presentation == LOOM_PRESENTATION_NEW_COMMIT &&
        boundary->presented_commit_id ==
            loom_ui_state.pending_commit_id) {
        loom_ui_advance_load();
        loom_ui_state.pending_load = LOOM_FALSE;
    }
}

static LoomStatus loom_ui_add_load_jobs(void)
{
    LoomAssetHandle handle;
    LoomCommitId commit_id;
    LoomDmaJob job;
    LoomStatus status;
    loom_u16 destination;
    loom_u16 byte_count;
    loom_u16 offset;
    loom_u16 remaining;
    loom_u8 destination_kind;
    loom_u8 count;

    loom_ui_current_segment(&handle, &destination, &byte_count,
                            &destination_kind);
    if (handle == LOOM_INVALID_HANDLE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    offset = loom_ui_state.load_offset;
    remaining = (loom_u16)(byte_count - offset);
    count = 0u;
    while (remaining != 0u && count < LOOM_UI_JOBS_PER_COMMIT) {
        loom_u16 bytes;

        bytes = remaining < LOOM_UI_DMA_CHUNK_BYTES
                    ? remaining
                    : LOOM_UI_DMA_CHUNK_BYTES;
        job.job_id = loom_ui_state.next_job_id;
        job.source_handle = handle;
        job.source_offset = offset;
        job.destination_offset = (loom_u16)(destination + offset);
        job.byte_count = bytes;
        job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
        job.destination_kind = destination_kind;
        job.policy = LOOM_DMA_REQUIRED;
        job.reserved = 0u;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_ui_state.next_job_id;
        ++count;
        offset = (loom_u16)(offset + bytes);
        remaining = (loom_u16)(remaining - bytes);
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_ui_state.pending_commit_id = commit_id;
    loom_ui_state.pending_load = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_ui_initialize(void)
{
    const LoomUiScreen *initial;
    LoomStatus status;

#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_initialize();
    }
#endif

    loom_ui_state.pending_commit_id = LOOM_COMMIT_NONE;
    loom_ui_state.active_screen = LOOM_UI_SCREEN_NONE;
    loom_ui_state.resident_scene = LOOM_UI_SCREEN_NONE;
    loom_ui_state.loading_scene = LOOM_UI_SCREEN_NONE;
    loom_ui_state.loading_screen = LOOM_UI_SCREEN_NONE;
    loom_ui_state.load_offset = 0u;
    loom_ui_state.next_job_id = 0x4000u;
    loom_ui_state.load_segment = LOOM_UI_LOAD_FONT;
    loom_ui_state.pending_load = LOOM_FALSE;
    loom_ui_state.common_ready = LOOM_FALSE;
    loom_ui_state.ready = LOOM_TRUE;
    loom_ui_state.initial_dismissed = LOOM_FALSE;
    loom_ui_state.initialized = LOOM_TRUE;
    if (loom_generated_ui_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    status = loom_ui_validate_catalog();
    if (status != LOOM_STATUS_OK) {
        loom_ui_state.initialized = LOOM_FALSE;
        return status;
    }
    if (loom_generated_ui_catalog.initial_screen !=
        LOOM_UI_SCREEN_NONE) {
        loom_ui_select(loom_generated_ui_catalog.initial_screen);
        initial = loom_ui_screen(loom_generated_ui_catalog.initial_screen);
        loom_ui_start_scene_load(initial->scene_index);
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_ui_update(const LoomInputSnapshot *input,
                          loom_u16 scene_index,
                          loom_u16 adventure_flags)
{
    const LoomUiScreen *screen;

#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        (void)scene_index;
        (void)adventure_flags;
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        if (input == (const LoomInputSnapshot *)0 ||
            input->pad_count == 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        /* A view answers only new presses: without one there is nothing to
         * look up, which is nearly every tick. */
        if (input->pads[0].pressed == 0u) {
            return LOOM_STATUS_OK;
        }
        return loom_project_ui_process_input(input);
    }
#endif

    if (loom_ui_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (input == (const LoomInputSnapshot *)0 ||
        input->pad_count == 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_generated_ui_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    if (loom_ui_state.resident_scene != scene_index &&
        loom_ui_state.loading_scene != scene_index) {
        loom_ui_start_scene_load(scene_index);
    }
    screen = loom_ui_screen(loom_ui_state.active_screen);
    if (loom_ui_state.initial_dismissed == LOOM_FALSE &&
        screen != (const LoomUiScreen *)0 &&
        (screen->flags & LOOM_UI_SCREEN_INITIAL) != 0u) {
        if ((input->pads[0].pressed & screen->dismiss_buttons) == 0u) {
            return LOOM_STATUS_OK;
        }
        loom_ui_state.initial_dismissed = LOOM_TRUE;
    }
    loom_ui_select(loom_ui_matching_screen(scene_index,
                                           adventure_flags));
    return LOOM_STATUS_OK;
}

LoomStatus loom_ui_build_load(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE &&
        loom_project_ui_state.initialized != LOOM_FALSE &&
        loom_project_ui_state.ready == LOOM_FALSE &&
        loom_project_ui_state.pending_load == LOOM_FALSE) {
        return loom_project_ui_add_load_jobs();
    }
#endif
    return LOOM_STATUS_OK;
}

LoomStatus loom_ui_build_frame(const LoomFrameBoundary *boundary,
                               LoomDisplayState *display)
{
    const LoomUiScreen *screen;

#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        if (boundary == (const LoomFrameBoundary *)0 ||
            display == (LoomDisplayState *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        return loom_project_ui_build_frame(boundary, display);
    }
#endif

    if (loom_ui_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (boundary == (const LoomFrameBoundary *)0 ||
        display == (LoomDisplayState *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_generated_ui_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    loom_ui_observe_boundary(boundary);
    if (loom_ui_state.ready == LOOM_FALSE) {
        display->brightness = 0u;
        display->main_layers = 0u;
        display->flags |= LOOM_DISPLAY_FORCED_BLANK;
        if (loom_ui_state.pending_load != LOOM_FALSE) {
            return LOOM_STATUS_OK;
        }
        return loom_ui_add_load_jobs();
    }
    screen = loom_ui_screen(loom_ui_state.active_screen);
    if (screen == (const LoomUiScreen *)0) {
        return LOOM_STATUS_OK;
    }
    display->bg_scroll_x[LOOM_UI_SCROLL_INDEX] =
        (screen->map_slot & 1u) != 0u ? 256 : 0;
    display->bg_scroll_y[LOOM_UI_SCROLL_INDEX] =
        (screen->map_slot & 2u) != 0u ? 256 : 0;
    if (loom_ui_blocks_gameplay() != LOOM_FALSE) {
        display->backdrop_color = LOOM_UI_PANEL_COLOR;
        display->main_layers = LOOM_UI_LAYER;
        display->flags |= LOOM_UI_DISPLAY_FLAGS;
        display->color_math_layers = 0u;
        display->color_math_flags = 0u;
    } else {
        display->main_layers |= LOOM_UI_LAYER;
        display->flags |= LOOM_UI_DISPLAY_FLAGS;
    }
    return LOOM_STATUS_OK;
}

loom_u8 loom_ui_blocks_gameplay(void)
{
    const LoomUiScreen *screen;

#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        const LoomUiProjectView *view;
        LoomProjectUiStackEntry *top;
        loom_u8 blocks;

        if (loom_project_ui_state.blocks_generation ==
            loom_project_ui_state.stack_generation) {
            return loom_project_ui_state.blocks_cached;
        }
        top = loom_project_ui_top();
        if (top == (LoomProjectUiStackEntry *)0) {
            blocks = LOOM_FALSE;
        } else if (top->dialogue_index != LOOM_UI_SCREEN_NONE) {
            blocks = LOOM_TRUE;
        } else {
            view = &loom_generated_project_ui_catalog.views[
                top->view_index];
            blocks = view->presentation == LOOM_UI_PRESENTATION_OVERLAY
                         ? LOOM_FALSE
                         : LOOM_TRUE;
        }
        loom_project_ui_state.blocks_cached = blocks;
        loom_project_ui_state.blocks_generation =
            loom_project_ui_state.stack_generation;
        return blocks;
    }
#endif

    screen = loom_ui_screen(loom_ui_state.active_screen);
    return screen != (const LoomUiScreen *)0 &&
                   (screen->flags &
                    LOOM_UI_SCREEN_BLOCKS_GAMEPLAY) != 0u
               ? LOOM_TRUE
               : LOOM_FALSE;
}

loom_u8 loom_ui_ready(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_state.ready;
    }
#endif
    return loom_ui_state.ready;
}

loom_u16 loom_ui_active_screen(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.view_index == LOOM_UI_SCREEN_NONE) {
            return LOOM_UI_SCREEN_NONE;
        }
        return loom_generated_project_ui_catalog.views[
            loom_project_ui_state.view_index].handle;
    }
#endif
    return loom_ui_state.active_screen;
}

LoomStatus loom_ui_set_bool(LoomUiBindingHandle binding, loom_u8 value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_BOOL, value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_set_u8(LoomUiBindingHandle binding, loom_u8 value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_U8, value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_set_u16(LoomUiBindingHandle binding, loom_u16 value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_U16, value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_set_enum(LoomUiBindingHandle binding, loom_u8 value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_ENUM, value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_set_icon(LoomUiBindingHandle binding,
                            LoomUiIconHandle value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_ICON, value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_set_message(LoomUiBindingHandle binding,
                               LoomUiMessageHandle value)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_set(binding, LOOM_UI_BINDING_MESSAGE,
                                   value);
    }
#else
    (void)binding;
    (void)value;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_push_view(LoomUiViewHandle view)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_show_view(view, LOOM_FALSE);
    }
#else
    (void)view;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_replace_view(LoomUiViewHandle view)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_show_view(view, LOOM_TRUE);
    }
#else
    (void)view;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_pop_view(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        if (loom_project_ui_busy() != LOOM_FALSE) {
            return LOOM_STATUS_BUSY;
        }
        return loom_project_ui_pop_internal();
    }
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_push_dialogue(LoomUiDialogueHandle dialogue)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        const LoomUiProjectBundle *bundle;
        const LoomUiProjectDialogue *record;
        const LoomUiProjectView *view;
        LoomProjectUiStackEntry *entry;
        LoomStatus status;
        loom_u16 dialogue_index;
        loom_u16 view_index;
        loom_u8 target_slot;

        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        if (loom_project_ui_busy() != LOOM_FALSE) {
            return LOOM_STATUS_BUSY;
        }
        if (loom_project_ui_state.stack_depth >=
            LOOM_UI_STACK_CAPACITY) {
            return LOOM_STATUS_CAPACITY;
        }
        bundle = &loom_generated_project_ui_catalog.bundles[
            loom_project_ui_state.bundle_index];
        dialogue_index = loom_project_ui_find_dialogue(dialogue, bundle);
        if (dialogue_index == LOOM_UI_SCREEN_NONE ||
            bundle->scratch_slot > 3u) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        record = &loom_generated_project_ui_catalog.dialogues[
            dialogue_index];
        view_index = loom_project_ui_find_view(record->view, bundle);
        if (view_index == LOOM_UI_SCREEN_NONE) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        view = &loom_generated_project_ui_catalog.views[view_index];
        target_slot = loom_project_ui_state.view_index !=
                          LOOM_UI_SCREEN_NONE &&
                      loom_project_ui_state.presented_map_slot ==
                          bundle->scratch_slot
                          ? view->map_slot
                          : bundle->scratch_slot;
        entry = &loom_project_ui_state.stack[
            loom_project_ui_state.stack_depth++];
        ++loom_project_ui_state.stack_generation;
        entry->view_index = view_index;
        entry->dialogue_index = dialogue_index;
        entry->page_index = record->first_page;
        entry->focus_index = 0u;
        entry->map_slot = target_slot;
        status = loom_project_ui_start_page(record->first_page,
                                            target_slot);
        if (status != LOOM_STATUS_OK) {
            --loom_project_ui_state.stack_depth;
            ++loom_project_ui_state.stack_generation;
            return status;
        }
        return LOOM_STATUS_OK;
    }
#else
    (void)dialogue;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_enqueue_command(LoomUiCommandHandle command)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        return loom_project_ui_enqueue(command);
    }
#else
    (void)command;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_ui_poll_command(LoomUiCommandHandle *command)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        if (command == (LoomUiCommandHandle *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (loom_project_ui_state.command_count == 0u) {
            return LOOM_STATUS_NOT_READY;
        }
        *command = loom_project_ui_state.commands[
            loom_project_ui_state.command_head];
        loom_project_ui_state.command_head =
            (loom_u8)((loom_project_ui_state.command_head + 1u) %
                      LOOM_UI_COMMAND_QUEUE_CAPACITY);
        --loom_project_ui_state.command_count;
        loom_project_ui_update_telemetry();
        return LOOM_STATUS_OK;
    }
#else
    (void)command;
#endif
    return LOOM_STATUS_UNSUPPORTED;
}

loom_u8 loom_ui_stack_depth(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_state.stack_depth;
    }
#endif
    return 0u;
}

loom_u8 loom_ui_focus_index(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        LoomProjectUiStackEntry *top;

        top = loom_project_ui_top();
        return top != (LoomProjectUiStackEntry *)0
                   ? top->focus_index
                   : 0u;
    }
#endif
    return 0u;
}

loom_u16 loom_ui_dialogue_page(void)
{
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        return loom_project_ui_state.telemetry.dialogue_page;
    }
#endif
    return LOOM_UI_SCREEN_NONE;
}

LoomStatus loom_ui_get_telemetry(LoomUiTelemetry *telemetry)
{
    if (telemetry == (LoomUiTelemetry *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
#if LOOM_GENERATED_PROJECT_UI_ENABLED
    if (loom_generated_project_ui_enabled != LOOM_FALSE) {
        if (loom_project_ui_state.initialized == LOOM_FALSE) {
            return LOOM_STATUS_NOT_READY;
        }
        /* Derived once per read rather than once per frame: only the debug
         * witness reads this, so a release cartridge never computes it. The
         * counters the boundary observer accumulates persist regardless. */
        loom_project_ui_update_telemetry();
        *telemetry = loom_project_ui_state.telemetry;
        return LOOM_STATUS_OK;
    }
#endif
    return LOOM_STATUS_UNSUPPORTED;
}
