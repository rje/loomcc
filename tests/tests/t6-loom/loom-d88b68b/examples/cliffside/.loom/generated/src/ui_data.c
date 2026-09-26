#include <loom/generated/ui.h>

const loom_u8 loom_generated_project_ui_enabled = LOOM_TRUE;


static loom_u16 loom_generated_ui_current_values[2u];
static loom_u16 loom_generated_ui_target_values[2u];
static loom_u16 loom_generated_ui_presented_values[2u];

static const LoomUiBindingRecord loom_generated_ui_bindings[2u] = {
    { (LoomUiBindingHandle)5853u, 8u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_U16, 0u },
    { (LoomUiBindingHandle)4244u, 8u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_U16, 0u },
};

static const LoomUiCommandHandle loom_generated_ui_commands[2u] = {
    (LoomUiCommandHandle)12547u,
    (LoomUiCommandHandle)12950u,
};


static const LoomUiProjectPatch loom_generated_ui_patches[1u] = {
    { (LoomAssetHandle)16u, 4u, 16u, 40u, 0u, (const loom_u16 *)0, 0u, 1u, 255u, 0u, 9u, 8u, 1u, 0u, LOOM_UI_PATCH_METER, LOOM_UI_PREDICATE_NONE, LOOM_UI_PADDING_NONE, 0u, 0u },
};

static const LoomUiProjectFocus loom_generated_ui_focus[3u] = {
    { (LoomUiCommandHandle)12547u, 555u, 2u, 1u, 0u },
    { (LoomUiCommandHandle)12547u, 489u, 150u, 1u, 0u },
    { (LoomUiCommandHandle)12950u, 585u, 152u, 1u, 0u },
};

static const LoomUiProjectInput loom_generated_ui_inputs[8u] = {
    { LOOM_BUTTON_UP, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_PREVIOUS, 0u },
    { LOOM_BUTTON_DOWN, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_NEXT, 0u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)12547u, LOOM_UI_INPUT_COMMAND, 1u },
    { LOOM_BUTTON_UP, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_PREVIOUS, 0u },
    { LOOM_BUTTON_DOWN, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_NEXT, 0u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)12547u, LOOM_UI_INPUT_COMMAND, 1u },
};

static const LoomUiProjectDialogueChoice loom_generated_ui_dialogue_choices[1u] = {
    { LOOM_UI_HANDLE_NONE, LOOM_UI_SCREEN_NONE, LOOM_UI_SCREEN_NONE, 0u, 0u }
};

static const LoomUiProjectDialoguePage loom_generated_ui_dialogue_pages[1u] = {
    { LOOM_INVALID_HANDLE, LOOM_UI_HANDLE_NONE, 0u, 0u, 0u, 0u }
};

static const LoomUiProjectDialogue loom_generated_ui_dialogues[1u] = {
    { LOOM_UI_HANDLE_NONE, LOOM_UI_HANDLE_NONE, 0u, 0u }
};

static const LoomUiProjectView loom_generated_ui_views[3u] = {
    { (LoomUiViewHandle)52065u, (LoomAssetHandle)13u, 2048u, 0u, 0u, 0u, 1u, 0u, 4u, 0u, 0u, LOOM_UI_PRESENTATION_REPLACE },
    { (LoomUiViewHandle)50644u, (LoomAssetHandle)14u, 2048u, 0u, 1u, 1u, 0u, 4u, 0u, 65535u, 1u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)50763u, (LoomAssetHandle)15u, 2048u, 1u, 0u, 1u, 2u, 4u, 4u, 148u, 2u, LOOM_UI_PRESENTATION_REPLACE },
};

static const LoomUiProjectBundle loom_generated_ui_bundles[1u] = {
    { (LoomUiBundleHandle)28232u, (LoomAssetHandle)11u, (LoomAssetHandle)12u, (LoomAssetHandle)16u, 1056u, 16u, 0u, 3u, 0u, 1u, 0u, 0u, 255u, 0u },
};

const LoomUiProjectCatalog loom_generated_project_ui_catalog = {
2u, 3u, 1u, 1u,
2u, 3u, 8u, 0u,
0u, 0u,
(LoomUiBundleHandle)28232u, (LoomUiViewHandle)52065u,
loom_generated_ui_bindings, loom_generated_ui_views,
loom_generated_ui_bundles, loom_generated_ui_patches,
loom_generated_ui_commands, loom_generated_ui_focus,
loom_generated_ui_inputs, loom_generated_ui_dialogues,
loom_generated_ui_dialogue_pages, loom_generated_ui_dialogue_choices,
loom_generated_ui_current_values, loom_generated_ui_target_values,
loom_generated_ui_presented_values
};
