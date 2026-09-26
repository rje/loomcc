#ifndef LOOM_INPUT_H
#define LOOM_INPUT_H

#include <loom/types.h>

#define LOOM_INPUT_PAD_CAPACITY 2u

/* Stable, active-high SNES joypad bit positions. */
#define LOOM_BUTTON_B ((loom_u16)0x8000u)
#define LOOM_BUTTON_Y ((loom_u16)0x4000u)
#define LOOM_BUTTON_SELECT ((loom_u16)0x2000u)
#define LOOM_BUTTON_START ((loom_u16)0x1000u)
#define LOOM_BUTTON_UP ((loom_u16)0x0800u)
#define LOOM_BUTTON_DOWN ((loom_u16)0x0400u)
#define LOOM_BUTTON_LEFT ((loom_u16)0x0200u)
#define LOOM_BUTTON_RIGHT ((loom_u16)0x0100u)
#define LOOM_BUTTON_A ((loom_u16)0x0080u)
#define LOOM_BUTTON_X ((loom_u16)0x0040u)
#define LOOM_BUTTON_L ((loom_u16)0x0020u)
#define LOOM_BUTTON_R ((loom_u16)0x0010u)

typedef struct LoomPadFrame {
    /*
     * held is the latest physical sample. pressed/released OR together every
     * corresponding edge since the preceding logical snapshot. The first
     * snapshot uses an all-released baseline.
     */
    loom_u16 held;
    loom_u16 pressed;
    loom_u16 released;
} LoomPadFrame;

typedef struct LoomInputSnapshot {
    LoomFrameId frame_id;
    loom_u8 pad_count;
    loom_u8 reserved;
    LoomPadFrame pads[LOOM_INPUT_PAD_CAPACITY];
} LoomInputSnapshot;

LOOM_STATIC_ASSERT(loom_pad_frame_is_six_bytes,
                   sizeof(LoomPadFrame) == 6u);
LOOM_STATIC_ASSERT(loom_input_snapshot_is_sixteen_bytes,
                   sizeof(LoomInputSnapshot) == 16u);

#endif
