#include <loom/generated/ui.h>

const loom_u8 loom_generated_project_ui_enabled = LOOM_TRUE;

static const loom_u16 loom_generated_ui_binding_4_values[] = { 54052u, 61137u, 61006u, 62459u };

static loom_u16 loom_generated_ui_current_values[10u];
static loom_u16 loom_generated_ui_target_values[10u];
static loom_u16 loom_generated_ui_presented_values[10u];

static const LoomUiBindingRecord loom_generated_ui_bindings[10u] = {
    { (LoomUiBindingHandle)65381u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
    { (LoomUiBindingHandle)60489u, 6u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_U16, 0u },
    { (LoomUiBindingHandle)39304u, 6u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_U16, 0u },
    { (LoomUiBindingHandle)14456u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_U8, 0u },
    { (LoomUiBindingHandle)62824u, 54052u, loom_generated_ui_binding_4_values, 4u, LOOM_UI_BINDING_MESSAGE, 0u },
    { (LoomUiBindingHandle)14909u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
    { (LoomUiBindingHandle)1297u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
    { (LoomUiBindingHandle)40668u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
    { (LoomUiBindingHandle)35376u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
    { (LoomUiBindingHandle)36u, 0u, (const loom_u16 *)0, 0u, LOOM_UI_BINDING_BOOL, 0u },
};

static const LoomUiCommandHandle loom_generated_ui_commands[8u] = {
    (LoomUiCommandHandle)26155u,
    (LoomUiCommandHandle)8981u,
    (LoomUiCommandHandle)27650u,
    (LoomUiCommandHandle)15769u,
    (LoomUiCommandHandle)29839u,
    (LoomUiCommandHandle)41173u,
    (LoomUiCommandHandle)23223u,
    (LoomUiCommandHandle)24322u,
};

static const loom_u16 loom_generated_ui_patch_2_values[] = { 54052u, 61137u, 61006u, 62459u };
static const loom_u16 loom_generated_ui_patch_5_values[] = { 54052u, 61137u, 61006u, 62459u };

static const LoomUiProjectPatch loom_generated_ui_patches[7u] = {
    { (LoomAssetHandle)44u, 0u, 72u, 618u, 1u, (const loom_u16 *)0, 3u, 255u, 3u, 0u, 2u, 12u, 3u, 0u, LOOM_UI_PATCH_VISIBILITY, LOOM_UI_PREDICATE_AT_LEAST, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)44u, 150u, 12u, 102u, 0u, (const loom_u16 *)0, 1u, 2u, 255u, 0u, 7u, 6u, 1u, 0u, LOOM_UI_PATCH_METER, LOOM_UI_PREDICATE_NONE, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)44u, 234u, 108u, 36u, 0u, loom_generated_ui_patch_2_values, 4u, 255u, 255u, 0u, 4u, 27u, 2u, 0u, LOOM_UI_PATCH_MESSAGE, LOOM_UI_PREDICATE_NONE, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)44u, 666u, 8u, 33u, 1u, (const loom_u16 *)0, 0u, 255u, 0u, 0u, 2u, 2u, 2u, 0u, LOOM_UI_PATCH_VISIBILITY, LOOM_UI_PREDICATE_IS_TRUE, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)48u, 0u, 12u, 102u, 0u, (const loom_u16 *)0, 1u, 2u, 255u, 0u, 7u, 6u, 1u, 0u, LOOM_UI_PATCH_METER, LOOM_UI_PREDICATE_NONE, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)48u, 84u, 108u, 36u, 0u, loom_generated_ui_patch_5_values, 4u, 255u, 255u, 0u, 4u, 27u, 2u, 0u, LOOM_UI_PATCH_MESSAGE, LOOM_UI_PREDICATE_NONE, LOOM_UI_PADDING_NONE, 0u, 0u },
    { (LoomAssetHandle)48u, 516u, 8u, 33u, 1u, (const loom_u16 *)0, 0u, 255u, 0u, 0u, 2u, 2u, 2u, 0u, LOOM_UI_PATCH_VISIBILITY, LOOM_UI_PREDICATE_IS_TRUE, LOOM_UI_PADDING_NONE, 0u, 0u },
};

static const LoomUiProjectFocus loom_generated_ui_focus[4u] = {
    { (LoomUiCommandHandle)27650u, 519u, 146u, 1u, 0u },
    { (LoomUiCommandHandle)24322u, 616u, 148u, 1u, 0u },
    { (LoomUiCommandHandle)15769u, 456u, 686u, 1u, 0u },
    { (LoomUiCommandHandle)29839u, 550u, 688u, 1u, 0u },
};

static const LoomUiProjectInput loom_generated_ui_inputs[18u] = {
    { LOOM_BUTTON_UP, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_PREVIOUS, 0u },
    { LOOM_BUTTON_DOWN, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_NEXT, 0u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_B, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_CANCEL, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)27650u, LOOM_UI_INPUT_COMMAND, 1u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_B, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_CLOSE, 0u },
    { LOOM_BUTTON_SELECT, (LoomUiCommandHandle)23223u, LOOM_UI_INPUT_COMMAND, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)24322u, LOOM_UI_INPUT_COMMAND, 0u },
    { LOOM_BUTTON_UP, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_PREVIOUS, 0u },
    { LOOM_BUTTON_DOWN, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_FOCUS_NEXT, 0u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_B, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_CANCEL, 0u },
    { LOOM_BUTTON_A, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_ACTIVATE, 0u },
    { LOOM_BUTTON_B, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_CLOSE, 0u },
    { LOOM_BUTTON_SELECT, (LoomUiCommandHandle)23223u, LOOM_UI_INPUT_COMMAND, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)24322u, LOOM_UI_INPUT_COMMAND, 0u },
    { LOOM_BUTTON_START, (LoomUiCommandHandle)65535u, LOOM_UI_INPUT_CLOSE, 0u },
};

static const LoomUiProjectDialogueChoice loom_generated_ui_dialogue_choices[3u] = {
    { (LoomUiCommandHandle)41173u, 112u, 690u, 1u, 0u },
    { (LoomUiCommandHandle)26155u, 112u, 692u, 1u, 0u },
    { (LoomUiCommandHandle)8981u, 112u, 694u, 1u, 0u },
};

static const LoomUiProjectDialoguePage loom_generated_ui_dialogue_pages[3u] = {
    { (LoomAssetHandle)41u, (LoomUiMessageHandle)60064u, 0u, 1u, 0u, 0u },
    { (LoomAssetHandle)42u, (LoomUiMessageHandle)39586u, 1u, 1u, 0u, 0u },
    { (LoomAssetHandle)43u, (LoomUiMessageHandle)15458u, 2u, 1u, 0u, 0u },
};

static const LoomUiProjectDialogue loom_generated_ui_dialogues[3u] = {
    { (LoomUiDialogueHandle)45821u, (LoomUiViewHandle)64933u, 0u, 1u },
    { (LoomUiDialogueHandle)13575u, (LoomUiViewHandle)64933u, 1u, 1u },
    { (LoomUiDialogueHandle)40859u, (LoomUiViewHandle)64933u, 2u, 1u },
};

static const LoomUiProjectView loom_generated_ui_views[13u] = {
    { (LoomUiViewHandle)7677u, (LoomAssetHandle)38u, 2048u, 0u, 1u, 0u, 2u, 0u, 5u, 144u, 0u, LOOM_UI_PRESENTATION_REPLACE },
    { (LoomUiViewHandle)64933u, (LoomAssetHandle)39u, 2048u, 1u, 3u, 2u, 0u, 5u, 4u, 682u, 1u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)60750u, (LoomAssetHandle)40u, 2048u, 4u, 0u, 2u, 2u, 9u, 4u, 684u, 2u, LOOM_UI_PRESENTATION_REPLACE },
    { (LoomUiViewHandle)64933u, (LoomAssetHandle)47u, 2048u, 4u, 3u, 4u, 0u, 13u, 4u, 532u, 0u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)25583u, (LoomAssetHandle)51u, 2048u, 7u, 0u, 4u, 0u, 17u, 0u, 65535u, 0u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)34307u, (LoomAssetHandle)54u, 2048u, 7u, 0u, 4u, 0u, 17u, 0u, 65535u, 0u, LOOM_UI_PRESENTATION_MODAL },
    { (LoomUiViewHandle)33722u, (LoomAssetHandle)55u, 2048u, 7u, 0u, 4u, 0u, 17u, 0u, 65535u, 1u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)58059u, (LoomAssetHandle)58u, 2048u, 7u, 0u, 4u, 0u, 17u, 0u, 65535u, 0u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)61104u, (LoomAssetHandle)59u, 2048u, 7u, 0u, 4u, 0u, 17u, 0u, 65535u, 1u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)57584u, (LoomAssetHandle)62u, 2048u, 7u, 0u, 4u, 0u, 17u, 1u, 65535u, 0u, LOOM_UI_PRESENTATION_REPLACE },
    { (LoomUiViewHandle)33758u, (LoomAssetHandle)63u, 2048u, 7u, 0u, 4u, 0u, 18u, 0u, 65535u, 1u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)3077u, (LoomAssetHandle)64u, 2048u, 7u, 0u, 4u, 0u, 18u, 0u, 65535u, 2u, LOOM_UI_PRESENTATION_OVERLAY },
    { (LoomUiViewHandle)30895u, (LoomAssetHandle)65u, 2048u, 7u, 0u, 4u, 0u, 18u, 0u, 65535u, 3u, LOOM_UI_PRESENTATION_OVERLAY },
};

static const LoomUiProjectBundle loom_generated_ui_bundles[6u] = {
    { (LoomUiBundleHandle)16124u, (LoomAssetHandle)36u, (LoomAssetHandle)37u, (LoomAssetHandle)44u, 1568u, 16u, 0u, 3u, 0u, 4u, 0u, 3u, 3u, 0u },
    { (LoomUiBundleHandle)31852u, (LoomAssetHandle)45u, (LoomAssetHandle)46u, (LoomAssetHandle)48u, 1568u, 16u, 3u, 1u, 4u, 3u, 3u, 0u, 255u, 0u },
    { (LoomUiBundleHandle)59829u, (LoomAssetHandle)49u, (LoomAssetHandle)50u, (LoomAssetHandle)65535u, 1056u, 16u, 4u, 1u, 7u, 0u, 3u, 0u, 255u, 0u },
    { (LoomUiBundleHandle)46706u, (LoomAssetHandle)52u, (LoomAssetHandle)53u, (LoomAssetHandle)65535u, 1056u, 16u, 5u, 2u, 7u, 0u, 3u, 0u, 255u, 0u },
    { (LoomUiBundleHandle)64195u, (LoomAssetHandle)56u, (LoomAssetHandle)57u, (LoomAssetHandle)65535u, 1056u, 16u, 7u, 2u, 7u, 0u, 3u, 0u, 255u, 0u },
    { (LoomUiBundleHandle)17512u, (LoomAssetHandle)60u, (LoomAssetHandle)61u, (LoomAssetHandle)65535u, 1056u, 16u, 9u, 4u, 7u, 0u, 3u, 0u, 255u, 0u },
};

const LoomUiProjectCatalog loom_generated_project_ui_catalog = {
10u, 13u, 6u, 7u,
8u, 4u, 18u, 3u,
3u, 3u,
(LoomUiBundleHandle)16124u, (LoomUiViewHandle)7677u,
loom_generated_ui_bindings, loom_generated_ui_views,
loom_generated_ui_bundles, loom_generated_ui_patches,
loom_generated_ui_commands, loom_generated_ui_focus,
loom_generated_ui_inputs, loom_generated_ui_dialogues,
loom_generated_ui_dialogue_pages, loom_generated_ui_dialogue_choices,
loom_generated_ui_current_values, loom_generated_ui_target_values,
loom_generated_ui_presented_values
};
