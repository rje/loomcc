#ifndef LOOM_AUDIO_H
#define LOOM_AUDIO_H

#include <loom/types.h>

typedef loom_u16 LoomAudioCueHandle;

#define LOOM_AUDIO_COMMAND_PLAY ((loom_u8)0u)
#define LOOM_AUDIO_COMMAND_STOP ((loom_u8)1u)
#define LOOM_AUDIO_BUS_MUSIC ((loom_u8)0u)
#define LOOM_AUDIO_BUS_EFFECTS ((loom_u8)1u)
#define LOOM_AUDIO_VOLUME_SILENT ((loom_u8)0u)
#define LOOM_AUDIO_VOLUME_FULL ((loom_u8)255u)
#define LOOM_AUDIO_PAN_LEFT ((loom_s8)-127)
#define LOOM_AUDIO_PAN_CENTER ((loom_s8)0)
#define LOOM_AUDIO_PAN_RIGHT ((loom_s8)127)
#define LOOM_AUDIO_CUE_KIND_MUSIC ((loom_u8)0u)
#define LOOM_AUDIO_CUE_KIND_EFFECT ((loom_u8)1u)

typedef struct LoomAudioCueRecord {
    LoomAudioCueHandle handle;
    loom_u8 kind;
    loom_u8 reserved;
} LoomAudioCueRecord;

LOOM_STATIC_ASSERT(loom_audio_cue_record_is_four_bytes,
                   sizeof(LoomAudioCueRecord) == 4u);

/*
 * serial identifies one enqueue operation across 16-bit wrap. frame_id records
 * its logical origin; neither value is tied to visual commit acceptance. PLAY
 * accepts MUSIC with a valid cue and centered pan, or EFFECTS with a valid cue
 * and pan -127..127. STOP accepts only MUSIC with invalid cue, zero volume,
 * and centered pan. Other combinations return LOOM_STATUS_UNSUPPORTED.
 */
typedef struct LoomAudioCueCommand {
    loom_u16 serial;
    LoomFrameId frame_id;
    LoomAudioCueHandle cue;
    loom_u8 command;
    loom_u8 bus;
    loom_u8 volume;
    loom_s8 pan;
} LoomAudioCueCommand;

LOOM_STATIC_ASSERT(loom_audio_cue_command_is_ten_bytes,
                   sizeof(LoomAudioCueCommand) == 10u);

typedef struct LoomAudioDebugState {
    LoomFrameId frame_id;
    LoomAudioCueHandle last_cue;
    LoomAudioCueHandle current_music_cue;
    loom_u16 last_serial;
    loom_u16 music_start_count;
    loom_u16 effect_dispatch_count;
    loom_u16 process_count;
    loom_u8 initialized;
    loom_u8 reserved;
} LoomAudioDebugState;

LOOM_STATIC_ASSERT(loom_audio_debug_state_is_sixteen_bytes,
                   sizeof(LoomAudioDebugState) == 16u);

/* Defined by generated audio_data.c. */
extern const loom_u8 loom_generated_audio_enabled;
extern const loom_u16 loom_generated_audio_cue_count;
extern const LoomAudioCueRecord loom_generated_audio_cues[];

/* Stable, backend-neutral command facade used by portable game code. */
LoomStatus loom_audio_initialize(void);
LoomStatus loom_audio_begin_frame(LoomFrameId frame_id);
LoomStatus loom_audio_play_music(LoomAudioCueHandle cue, loom_u8 volume);
LoomStatus loom_audio_stop_music(void);
LoomStatus loom_audio_play_effect(LoomAudioCueHandle cue,
                                  loom_u8 volume,
                                  loom_s8 pan);
LoomStatus loom_audio_process(void);

/* Inspectable stable-cue telemetry retained only by debug cartridge builds. */
#if defined(LOOM_BUILD_DEBUG)
extern volatile LoomAudioDebugState loom_audio_debug_state;
#endif

#endif
