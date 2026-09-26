#include <loom/audio.h>
#include <loom/generated/audio_cues.h>

void on_crossing_welcome(void)
{
    (void)loom_audio_play_music(
        LOOM_GENERATED_AUDIO_CUE_ID_7C0EF0DC379C7549AE7102B4BD92C762,
        192u);
    (void)loom_audio_play_effect(
        LOOM_GENERATED_AUDIO_CUE_ID_D45F3214B19D52A167F1AF73C80D39D4,
        LOOM_AUDIO_VOLUME_FULL,
        LOOM_AUDIO_PAN_CENTER);
}
