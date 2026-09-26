#ifndef LOOM_UI_H
#define LOOM_UI_H

#include <loom/frame.h>
#include <loom/input.h>
/* The UI's background decides its VRAM, CGRAM, display layer and scroll pair.
 * Every translation unit that touches the UI must agree on it, so the header
 * pulls the generated answer rather than leaving each one to remember. */
#ifndef LOOM_UI_WITHOUT_GENERATED_CONFIG
#include <loom/generated/runtime_config.h>
#endif

#ifndef LOOM_GENERATED_PROJECT_UI_ENABLED
#define LOOM_GENERATED_PROJECT_UI_ENABLED 0
#endif

#define LOOM_UI_SCREEN_NONE ((loom_u16)0xffffu)
#define LOOM_UI_SCREEN_INITIAL ((loom_u8)0x01u)
#define LOOM_UI_SCREEN_BLOCKS_GAMEPLAY ((loom_u8)0x02u)

/* Where the cartridge UI lives depends on the background it is resident on,
 * which the project chooses and generation writes into runtime_config.h.
 * A host build without generated config keeps the BG2 addresses. */
#ifndef LOOM_GENERATED_UI_FONT_VRAM_BYTE
#define LOOM_GENERATED_UI_FONT_VRAM_BYTE 0x6000u
#endif
#ifndef LOOM_GENERATED_UI_MAP_VRAM_BYTE
#define LOOM_GENERATED_UI_MAP_VRAM_BYTE 0x8000u
#endif
#ifndef LOOM_GENERATED_UI_PALETTE_CGRAM_BYTE
#define LOOM_GENERATED_UI_PALETTE_CGRAM_BYTE 0x00c0u
#endif
#ifndef LOOM_GENERATED_UI_TILE_BITPLANES
#define LOOM_GENERATED_UI_TILE_BITPLANES 4u
#endif

#define LOOM_UI_FONT_VRAM_BYTE_ADDRESS ((loom_u16)LOOM_GENERATED_UI_FONT_VRAM_BYTE)
#define LOOM_UI_MAP_VRAM_BYTE_ADDRESS ((loom_u16)LOOM_GENERATED_UI_MAP_VRAM_BYTE)
#define LOOM_UI_PALETTE_CGRAM_BYTE_ADDRESS ((loom_u16)LOOM_GENERATED_UI_PALETTE_CGRAM_BYTE)
/* Two on BG3, four on BG1 and BG2: how many bytes one UI tile occupies. */
#define LOOM_UI_TILE_BYTES ((loom_u16)(LOOM_GENERATED_UI_TILE_BITPLANES * 8u))
/* A bundle carries two palettes, one per text style. Their size follows the
 * bitplanes: sixteen colours each on BG1 and BG2, four on BG3. */
#define LOOM_UI_BUNDLE_PALETTE_BYTES \
    ((loom_u16)(2u * (1u << LOOM_GENERATED_UI_TILE_BITPLANES) * 2u))

/* Plain integers: these are read by #if, where a cast is not an expression. */
#ifndef LOOM_GENERATED_UI_BACKGROUND
#define LOOM_GENERATED_UI_BACKGROUND 2
#endif

/* Which display layer the UI enables, which scroll pair it drives, and
 * whether Mode 1 must lift BG3 above everything so a HUD stays readable. */
#if LOOM_GENERATED_UI_BACKGROUND == 3
#define LOOM_UI_LAYER LOOM_LAYER_BG3
#define LOOM_UI_SCROLL_INDEX 2
#define LOOM_UI_DISPLAY_FLAGS LOOM_DISPLAY_MODE1_BG3_PRIORITY
#else
#define LOOM_UI_LAYER LOOM_LAYER_BG2
#define LOOM_UI_SCROLL_INDEX 1
#define LOOM_UI_DISPLAY_FLAGS ((loom_u8)0u)
#endif
#define LOOM_UI_MAP_BYTES ((loom_u16)2048u)
#define LOOM_UI_PANEL_COLOR ((loom_u16)0x20c4u)

#define LOOM_UI_INDEX_NONE ((loom_u8)0xffu)
#define LOOM_UI_HANDLE_NONE ((loom_u16)0xffffu)

#define LOOM_UI_BINDING_BOOL ((loom_u8)0u)
#define LOOM_UI_BINDING_U8 ((loom_u8)1u)
#define LOOM_UI_BINDING_U16 ((loom_u8)2u)
#define LOOM_UI_BINDING_ENUM ((loom_u8)3u)
#define LOOM_UI_BINDING_ICON ((loom_u8)4u)
#define LOOM_UI_BINDING_MESSAGE ((loom_u8)5u)

#define LOOM_UI_PRESENTATION_OVERLAY ((loom_u8)0u)
#define LOOM_UI_PRESENTATION_MODAL ((loom_u8)1u)
#define LOOM_UI_PRESENTATION_REPLACE ((loom_u8)2u)

#define LOOM_UI_PATCH_VISIBILITY ((loom_u8)0u)
#define LOOM_UI_PATCH_ICON ((loom_u8)1u)
#define LOOM_UI_PATCH_MESSAGE ((loom_u8)2u)
#define LOOM_UI_PATCH_NUMBER_DECIMAL ((loom_u8)3u)
#define LOOM_UI_PATCH_NUMBER_HEXADECIMAL ((loom_u8)4u)
#define LOOM_UI_PATCH_METER ((loom_u8)5u)

#define LOOM_UI_PREDICATE_IS_TRUE ((loom_u8)0u)
#define LOOM_UI_PREDICATE_IS_FALSE ((loom_u8)1u)
#define LOOM_UI_PREDICATE_EQUAL ((loom_u8)2u)
#define LOOM_UI_PREDICATE_NOT_EQUAL ((loom_u8)3u)
#define LOOM_UI_PREDICATE_LESS_THAN ((loom_u8)4u)
#define LOOM_UI_PREDICATE_AT_LEAST ((loom_u8)5u)
#define LOOM_UI_PREDICATE_NONE ((loom_u8)0xffu)

#define LOOM_UI_PADDING_NONE ((loom_u8)0u)
#define LOOM_UI_PADDING_ZERO ((loom_u8)1u)
#define LOOM_UI_PADDING_SPACE ((loom_u8)2u)

#define LOOM_UI_STACK_CAPACITY ((loom_u8)4u)
#define LOOM_UI_COMMAND_QUEUE_CAPACITY ((loom_u8)4u)

#define LOOM_UI_INPUT_FOCUS_PREVIOUS ((loom_u8)0u)
#define LOOM_UI_INPUT_FOCUS_NEXT ((loom_u8)1u)
#define LOOM_UI_INPUT_ACTIVATE ((loom_u8)2u)
#define LOOM_UI_INPUT_CANCEL ((loom_u8)3u)
#define LOOM_UI_INPUT_CLOSE ((loom_u8)4u)
#define LOOM_UI_INPUT_COMMAND ((loom_u8)5u)

typedef loom_u16 LoomUiBindingHandle;
typedef loom_u16 LoomUiCommandHandle;
typedef loom_u16 LoomUiIconHandle;
typedef loom_u16 LoomUiMessageHandle;
typedef loom_u16 LoomUiDialogueHandle;
typedef loom_u16 LoomUiViewHandle;
typedef loom_u16 LoomUiBundleHandle;

typedef struct LoomUiBindingRecord {
    LoomUiBindingHandle handle;
    loom_u16 default_value;
    const loom_u16 *allowed_values;
    loom_u16 allowed_count;
    loom_u8 type;
    loom_u8 reserved;
} LoomUiBindingRecord;

typedef struct LoomUiProjectPatch {
    LoomAssetHandle source_handle;
    loom_u16 source_offset;
    loom_u16 variant_stride;
    loom_u16 destination_word;
    loom_u16 predicate_value;
    const loom_u16 *variant_values;
    loom_u8 binding_index;
    loom_u8 secondary_binding_index;
    loom_u8 predicate_binding_index;
    loom_u8 reserved_index;
    loom_u16 variant_count;
    loom_u8 row_words;
    loom_u8 row_count;
    loom_u8 digits;
    loom_u8 kind;
    loom_u8 predicate;
    loom_u8 padding;
    loom_u8 reserved0;
    loom_u8 reserved1;
} LoomUiProjectPatch;

typedef struct LoomUiProjectView {
    LoomUiViewHandle handle;
    LoomAssetHandle map_handle;
    loom_u16 map_bytes;
    loom_u16 first_patch;
    loom_u16 patch_count;
    loom_u16 first_focus;
    loom_u16 focus_count;
    loom_u16 first_input;
    loom_u16 input_count;
    loom_u16 focus_cursor_source_offset;
    loom_u8 map_slot;
    loom_u8 presentation;
} LoomUiProjectView;

typedef struct LoomUiProjectFocus {
    LoomUiCommandHandle command;
    loom_u16 destination_word;
    loom_u16 blank_source_offset;
    loom_u8 close_after_command;
    loom_u8 reserved;
} LoomUiProjectFocus;

typedef struct LoomUiProjectInput {
    loom_u16 button_mask;
    LoomUiCommandHandle command;
    loom_u8 response;
    loom_u8 close_after_command;
} LoomUiProjectInput;

typedef struct LoomUiProjectDialogueChoice {
    LoomUiCommandHandle command;
    loom_u16 cursor_destination_word;
    loom_u16 blank_source_offset;
    loom_u8 close_after_command;
    loom_u8 reserved;
} LoomUiProjectDialogueChoice;

typedef struct LoomUiProjectDialoguePage {
    LoomAssetHandle map_handle;
    LoomUiMessageHandle message;
    loom_u16 first_choice;
    loom_u16 choice_count;
    loom_u8 continuation;
    loom_u8 reserved;
} LoomUiProjectDialoguePage;

typedef struct LoomUiProjectDialogue {
    LoomUiDialogueHandle handle;
    LoomUiViewHandle view;
    loom_u16 first_page;
    loom_u16 page_count;
} LoomUiProjectDialogue;

typedef struct LoomUiProjectBundle {
    LoomUiBundleHandle handle;
    LoomAssetHandle tiles_handle;
    LoomAssetHandle palette_handle;
    LoomAssetHandle patch_handle;
    loom_u16 tiles_bytes;
    loom_u16 palette_bytes;
    loom_u16 first_view;
    loom_u16 view_count;
    loom_u16 first_patch;
    loom_u16 patch_count;
    loom_u16 first_dialogue;
    loom_u16 dialogue_count;
    loom_u8 scratch_slot;
    loom_u8 reserved;
} LoomUiProjectBundle;

typedef struct LoomUiProjectCatalog {
    loom_u16 binding_count;
    loom_u16 view_count;
    loom_u16 bundle_count;
    loom_u16 patch_count;
    loom_u16 command_count;
    loom_u16 focus_count;
    loom_u16 input_count;
    loom_u16 dialogue_count;
    loom_u16 dialogue_page_count;
    loom_u16 dialogue_choice_count;
    LoomUiBundleHandle boot_bundle;
    LoomUiViewHandle boot_view;
    const LoomUiBindingRecord *bindings;
    const LoomUiProjectView *views;
    const LoomUiProjectBundle *bundles;
    const LoomUiProjectPatch *patches;
    const LoomUiCommandHandle *commands;
    const LoomUiProjectFocus *focus;
    const LoomUiProjectInput *inputs;
    const LoomUiProjectDialogue *dialogues;
    const LoomUiProjectDialoguePage *dialogue_pages;
    const LoomUiProjectDialogueChoice *dialogue_choices;
    loom_u16 *current_values;
    loom_u16 *target_values;
    loom_u16 *presented_values;
} LoomUiProjectCatalog;

typedef struct LoomUiTelemetry {
    loom_u16 accepted_commits;
    loom_u16 repeated_boundaries;
    loom_u16 dialogue_page;
    loom_u16 last_commit_bytes;
    LoomUiViewHandle active_view;
    loom_u8 stack_depth;
    loom_u8 focus_index;
    loom_u8 commands_queued;
    loom_u8 last_commit_jobs;
    loom_u8 transition_pending;
    loom_u8 presented_map_slot;
    loom_u8 blocks_gameplay;
    loom_u8 reserved;
} LoomUiTelemetry;

LOOM_STATIC_ASSERT(loom_ui_telemetry_is_eighteen_bytes,
                   sizeof(LoomUiTelemetry) == 18u);

typedef struct LoomUiScreen {
    LoomAssetHandle map_handle;
    loom_u16 map_bytes;
    loom_u16 scene_index;
    loom_u16 required_flags;
    loom_u16 forbidden_flags;
    loom_u16 dismiss_buttons;
    loom_u8 flags;
    loom_u8 map_slot;
} LoomUiScreen;

typedef struct LoomUiCatalog {
    LoomAssetHandle font_tiles_handle;
    LoomAssetHandle palette_handle;
    loom_u16 font_tiles_bytes;
    loom_u16 palette_bytes;
    loom_u16 screen_count;
    loom_u16 initial_screen;
    const LoomUiScreen *screens;
} LoomUiCatalog;

/* Defined by generated mode1_data.c. */
extern const loom_u8 loom_generated_ui_enabled;
extern const LoomUiCatalog loom_generated_ui_catalog;

/* Defined by generated ui_data.c only when project-owned UI is enabled. */
#if LOOM_GENERATED_PROJECT_UI_ENABLED
extern const loom_u8 loom_generated_project_ui_enabled;
extern const LoomUiProjectCatalog loom_generated_project_ui_catalog;
#endif

LoomStatus loom_ui_initialize(void);
LoomStatus loom_ui_update(const LoomInputSnapshot *input,
                          loom_u16 scene_index,
                          loom_u16 adventure_flags);
LoomStatus loom_ui_build_frame(const LoomFrameBoundary *boundary,
                               LoomDisplayState *display);
/* The WRAM block a number patch's composed digits are sent from: a whole
 * decimal number is one job rather than a job per digit. Defined only when
 * the project has a cartridge UI (LOOM_GENERATED_PROJECT_UI_ENABLED). */
#define LOOM_UI_BLOCK_HANDLE ((LoomWramBlockHandle)2u)
const loom_u8 *loom_ui_block(loom_u16 *bytes);

/* A project UI's first load: the font, the palette and every view's map,
 * sent while the screen is dark. The frame build calls it after everything
 * else in the commit, and it spends whatever jobs and bytes are left. */
LoomStatus loom_ui_build_load(void);
loom_u8 loom_ui_blocks_gameplay(void);
loom_u8 loom_ui_ready(void);
loom_u16 loom_ui_active_screen(void);

LoomStatus loom_ui_set_bool(LoomUiBindingHandle binding, loom_u8 value);
LoomStatus loom_ui_set_u8(LoomUiBindingHandle binding, loom_u8 value);
LoomStatus loom_ui_set_u16(LoomUiBindingHandle binding, loom_u16 value);
LoomStatus loom_ui_set_enum(LoomUiBindingHandle binding, loom_u8 value);
LoomStatus loom_ui_set_icon(LoomUiBindingHandle binding,
                            LoomUiIconHandle value);
LoomStatus loom_ui_set_message(LoomUiBindingHandle binding,
                               LoomUiMessageHandle value);
LoomStatus loom_ui_push_view(LoomUiViewHandle view);
LoomStatus loom_ui_replace_view(LoomUiViewHandle view);
LoomStatus loom_ui_pop_view(void);
LoomStatus loom_ui_push_dialogue(LoomUiDialogueHandle dialogue);
LoomStatus loom_ui_poll_command(LoomUiCommandHandle *command);
/*
 * Puts a polled command back at the end of the queue. The generated schedule
 * uses this to take the commands that carry a built-in action and hand the
 * rest on to the game's own hook unchanged.
 */
LoomStatus loom_ui_enqueue_command(LoomUiCommandHandle command);
loom_u8 loom_ui_stack_depth(void);
loom_u8 loom_ui_focus_index(void);
loom_u16 loom_ui_dialogue_page(void);
LoomStatus loom_ui_get_telemetry(LoomUiTelemetry *telemetry);

#endif
