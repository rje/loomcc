#include <loom/audio.h>
#include <loom/port.h>

#if defined(LOOM_BUILD_DEBUG)
volatile LoomAudioDebugState loom_audio_debug_state;
#else
static LoomAudioDebugState loom_audio_release_state;
#define loom_audio_debug_state loom_audio_release_state
#endif

static loom_u16 loom_audio_next_serial;

static const LoomAudioCueRecord *loom_audio_find_cue(
    LoomAudioCueHandle handle)
{
    const LoomAudioCueRecord *cue;
    loom_u16 remaining;

    cue = loom_generated_audio_cues;
    remaining = loom_generated_audio_cue_count;
    while (remaining != 0u) {
        if (cue->handle == handle) {
            return cue;
        }
        ++cue;
        --remaining;
    }
    return (const LoomAudioCueRecord *)0;
}

static void loom_audio_increment(volatile loom_u16 *value)
{
    if (*value != 0xffffu) {
        ++*value;
    }
}

static LoomStatus loom_audio_enqueue(LoomAudioCueHandle handle,
                                     loom_u8 command_kind,
                                     loom_u8 bus,
                                     loom_u8 volume,
                                     loom_s8 pan)
{
    LoomAudioCueCommand command;
    LoomStatus status;

    command.serial = loom_audio_next_serial;
    command.frame_id = loom_audio_debug_state.frame_id;
    command.cue = handle;
    command.command = command_kind;
    command.bus = bus;
    command.volume = volume;
    command.pan = pan;
    status = loom_port_audio_enqueue(&command);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_audio_debug_state.last_serial = loom_audio_next_serial;
    ++loom_audio_next_serial;
    loom_audio_debug_state.last_cue = handle;
    return LOOM_STATUS_OK;
}

LoomStatus loom_audio_initialize(void)
{
    const LoomAudioCueRecord *cue;
    loom_u16 remaining;

    loom_audio_next_serial = 0u;
    loom_audio_debug_state.frame_id = 0u;
    loom_audio_debug_state.last_cue = LOOM_INVALID_HANDLE;
    loom_audio_debug_state.current_music_cue = LOOM_INVALID_HANDLE;
    loom_audio_debug_state.last_serial = 0u;
    loom_audio_debug_state.music_start_count = 0u;
    loom_audio_debug_state.effect_dispatch_count = 0u;
    loom_audio_debug_state.process_count = 0u;
    loom_audio_debug_state.initialized = LOOM_FALSE;
    loom_audio_debug_state.reserved = 0u;
    if (loom_generated_audio_enabled == LOOM_FALSE) {
        if (loom_generated_audio_cue_count != 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        loom_audio_debug_state.initialized = LOOM_TRUE;
        return LOOM_STATUS_OK;
    }
    if (loom_generated_audio_cue_count == 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    cue = loom_generated_audio_cues;
    remaining = loom_generated_audio_cue_count;
    while (remaining != 0u) {
        if (cue->handle == LOOM_INVALID_HANDLE || cue->reserved != 0u ||
            cue->kind > LOOM_AUDIO_CUE_KIND_EFFECT) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        ++cue;
        --remaining;
    }
    loom_audio_debug_state.initialized = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_audio_begin_frame(LoomFrameId frame_id)
{
    if (loom_audio_debug_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    loom_audio_debug_state.frame_id = frame_id;
    return LOOM_STATUS_OK;
}

LoomStatus loom_audio_play_music(LoomAudioCueHandle handle,
                                 loom_u8 volume)
{
    const LoomAudioCueRecord *cue;
    LoomStatus status;

    if (loom_audio_debug_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    cue = loom_audio_find_cue(handle);
    if (cue == (const LoomAudioCueRecord *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (cue->kind != LOOM_AUDIO_CUE_KIND_MUSIC) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    status = loom_audio_enqueue(handle, LOOM_AUDIO_COMMAND_PLAY,
                                LOOM_AUDIO_BUS_MUSIC, volume,
                                LOOM_AUDIO_PAN_CENTER);
    if (status == LOOM_STATUS_OK) {
        loom_audio_debug_state.current_music_cue = handle;
        loom_audio_increment(&loom_audio_debug_state.music_start_count);
    }
    return status;
}

LoomStatus loom_audio_stop_music(void)
{
    LoomStatus status;

    if (loom_audio_debug_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    status = loom_audio_enqueue(LOOM_INVALID_HANDLE,
                                LOOM_AUDIO_COMMAND_STOP,
                                LOOM_AUDIO_BUS_MUSIC,
                                LOOM_AUDIO_VOLUME_SILENT,
                                LOOM_AUDIO_PAN_CENTER);
    if (status == LOOM_STATUS_OK) {
        loom_audio_debug_state.current_music_cue = LOOM_INVALID_HANDLE;
    }
    return status;
}

LoomStatus loom_audio_play_effect(LoomAudioCueHandle handle,
                                  loom_u8 volume,
                                  loom_s8 pan)
{
    const LoomAudioCueRecord *cue;
    LoomStatus status;

    if (loom_audio_debug_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    cue = loom_audio_find_cue(handle);
    if (cue == (const LoomAudioCueRecord *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (cue->kind != LOOM_AUDIO_CUE_KIND_EFFECT || pan == (loom_s8)-128) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    status = loom_audio_enqueue(handle, LOOM_AUDIO_COMMAND_PLAY,
                                LOOM_AUDIO_BUS_EFFECTS, volume, pan);
    if (status == LOOM_STATUS_OK) {
        loom_audio_increment(&loom_audio_debug_state.effect_dispatch_count);
    }
    return status;
}

LoomStatus loom_audio_process(void)
{
    LoomStatus status;

    if (loom_audio_debug_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    status = loom_port_audio_process();
    if (status == LOOM_STATUS_OK) {
        loom_audio_increment(&loom_audio_debug_state.process_count);
    }
    return status;
}
