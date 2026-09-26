#ifndef LOOM_PORT_H
#define LOOM_PORT_H

#include <loom/assets.h>
#include <loom/audio.h>
#include <loom/input.h>
#include <loom/memory.h>
#include <loom/video.h>

#define LOOM_PORT_CONTRACT_VERSION ((loom_u16)1u)

/* Implemented by Loom's portable frame shell; adapter startup calls it. */
void loom_runtime_main(void);

/* Implemented by exactly one selected target adapter. */
LoomStatus loom_port_init(void);
LoomStatus loom_port_frame_wait(LoomFrameBoundary *frame,
                                LoomInputSnapshot *input);
LoomStatus loom_port_frame_begin(const LoomFrameCommit *commit);
LoomStatus loom_port_oam_stage(const LoomOamEntry *entry);
LoomStatus loom_port_dma_stage(const LoomDmaJob *job);
LoomStatus loom_port_frame_commit(void);
void loom_port_frame_abort(void);

LoomStatus loom_port_asset_read(const LoomAssetSpan *source,
                                loom_u8 *destination,
                                loom_u16 destination_capacity);
LoomStatus loom_port_wram_read(const LoomWramSpan *source,
                               loom_u8 *destination,
                               loom_u16 destination_capacity);
LoomStatus loom_port_wram_write(const LoomWramSpan *destination,
                                const loom_u8 *source,
                                loom_u16 source_length);

/* Battery-backed SRAM, addressed from zero for `length` bytes. A cartridge
 * without SRAM returns LOOM_STATUS_UNSUPPORTED; a span past the declared
 * size returns LOOM_STATUS_OUT_OF_RANGE. */
LoomStatus loom_port_sram_read(loom_u16 offset,
                               loom_u8 *destination,
                               loom_u16 length);
LoomStatus loom_port_sram_write(loom_u16 offset,
                                const loom_u8 *source,
                                loom_u16 length);

LoomStatus loom_port_audio_enqueue(const LoomAudioCueCommand *command);
LoomStatus loom_port_audio_process(void);

#endif
