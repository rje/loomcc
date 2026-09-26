#include <loom/audio.h>
#include <loom/combat.h>
#include <loom/movement.h>
#include <loom/ui.h>
#include <loom/variables.h>
#include <loom/generated/audio_cues.h>
#include <loom/generated/ui.h>
#include <loom/generated/user_hooks.h>
#include <loom/generated/variables.h>

/*
 * Cliffside's own code: the sounds a platformer makes and the summit.
 *
 * Everything else -- the run and jump, the walkers, the ferry, the gems
 * that count themselves into a variable, the health that ends the run --
 * is Loom's, authored in the editor. This file listens: once a tick it
 * compares what the body and the counters say with what they said last
 * tick and plays the sound for what changed, and the summit's trigger
 * shows the view that ends the climb.
 */

/* The imported sound cues by what they are for. scripts/samples/cliffside.py
 * writes this block from Assets/Audio's cue IDs, which are stable-ID based
 * so that renaming a cue cannot silently change which sound plays. */
/* sounds: begin */
#define CLIFF_SOUND_JUMP LOOM_GENERATED_AUDIO_CUE_ID_612F1EDF1D025A31569954FFBC3DE721
#define CLIFF_SOUND_GEM LOOM_GENERATED_AUDIO_CUE_ID_B302A91845AC680A627EBF3F8497D2B8
#define CLIFF_SOUND_STOMP LOOM_GENERATED_AUDIO_CUE_ID_A1CD60D36FFAA4EFC14080ED2A69CFE2
#define CLIFF_SOUND_HURT LOOM_GENERATED_AUDIO_CUE_ID_AA9CAA1466BAE6A838AA9EBA1959C670
#define CLIFF_SOUND_SUMMIT LOOM_GENERATED_AUDIO_CUE_ID_BAF9AECD141BE870D00F3355D0DED65F
/* sounds: end */

static loom_u8 was_grounded;
static loom_u16 last_gems;
static loom_u16 last_health;
static loom_u16 last_stomps;

static void play(LoomAudioCueHandle cue)
{
    (void)loom_audio_play_effect(cue, LOOM_AUDIO_VOLUME_FULL, LOOM_AUDIO_PAN_CENTER);
}

void update_player(void)
{
    loom_u8 grounded;
    loom_u16 gems;
    loom_u16 health;
    loom_u16 stomps;

    grounded = loom_movement_on_ground();
    if (was_grounded != LOOM_FALSE && grounded == LOOM_FALSE &&
        loom_movement_velocity_y() < 0) {
        /* Left the ground rising: a jump, not a walk off an edge. */
        play(CLIFF_SOUND_JUMP);
    }
    was_grounded = grounded;

    /* A restart puts the counters back, so only a rise is news. */
    stomps = loom_combat_stomp_count();
    if (stomps > last_stomps) {
        play(CLIFF_SOUND_STOMP);
    }
    last_stomps = stomps;
    /* The variables' words, read in place: loom_variable_get looks the
     * handle up, which costs more than the rest of this hook. */
    gems = LOOM_VAR_GEMS_WORD;
    if (gems > last_gems) {
        play(CLIFF_SOUND_GEM);
    }
    last_gems = gems;
    health = LOOM_VAR_HEALTH_WORD;
    if (health < last_health) {
        play(CLIFF_SOUND_HURT);
    }
    last_health = health;
}

/* The summit flag's trigger: the climb is done. */
void on_summit(void)
{
    play(CLIFF_SOUND_SUMMIT);
    (void)loom_ui_replace_view(LOOM_UI_VIEW_SUMMIT);
}
