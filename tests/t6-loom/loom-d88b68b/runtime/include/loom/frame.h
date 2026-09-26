#ifndef LOOM_FRAME_H
#define LOOM_FRAME_H

#include <loom/pools.h>
#include <loom/port.h>

/* Fixed limits qualified by the selected PVSnesLib capability; the OAM
 * record capacity is LOOM_FRAME_OAM_CAPACITY from loom/pools.h. A commit may
 * carry up to sixteen required jobs and one kilobyte of VBlank transfers;
 * resident loads and UI patches pace themselves well below that so their
 * timing is unchanged, while map streaming uses the headroom. */
#define LOOM_FRAME_DMA_CAPACITY ((loom_u8)16u)
#define LOOM_FRAME_REQUIRED_DMA_BYTES_MAX ((loom_u16)1024u)

/*
 * Portable build-side frame intent. Generated profile phases fill this state;
 * submit copies it through the target-port transaction in deterministic order.
 */
LoomStatus loom_frame_build_initialize(void);
LoomStatus loom_frame_build_begin(void);
LoomStatus loom_frame_build_set_display(const LoomDisplayState *display);
LoomStatus loom_frame_build_set_raster(const LoomRasterBinding *raster);
LoomStatus loom_frame_build_add_oam(const LoomOamEntry *entry);
/* In-place access to the open commit, so profiles write fields without
 * building a local record and copying it: null when no commit is open. */
LoomDisplayState *loom_frame_build_display(void);
LoomRasterBinding *loom_frame_build_raster(void);
/* Reserves the next OAM record of the open commit: null when closed or full. */
LoomOamEntry *loom_frame_build_reserve_oam(void);
LoomStatus loom_frame_build_add_dma(const LoomDmaJob *job);
/* The next DMA job slot of the open build, written in place, or null when
 * the build is closed or full: loom_frame_build_add_dma without the copy. */
LoomDmaJob *loom_frame_build_reserve_dma(void);
/* The open build's next free DMA job slots, for a writer that fills several
 * in place (the console's surface flush), and the count it filled. */
LoomDmaJob *loom_frame_build_dma_space(loom_u8 *room);
void loom_frame_build_dma_advance(loom_u8 count);
/* Bytes the open build's DMA jobs carry so far. */
loom_u16 loom_frame_build_dma_bytes(void);
LoomStatus loom_frame_build_current_commit_id(LoomCommitId *commit_id);
LoomStatus loom_frame_build_submit(void);
void loom_frame_build_abort(void);

#endif
