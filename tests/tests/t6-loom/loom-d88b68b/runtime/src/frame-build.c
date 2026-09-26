#include <loom/frame.h>
#include <loom/runtime.h>

typedef struct LoomFrameBuildState {
    LoomFrameCommit commit;
    LoomOamEntry oam[LOOM_FRAME_OAM_CAPACITY];
    LoomDmaJob dma[LOOM_FRAME_DMA_CAPACITY];
    LoomDmaJob *next_dma;
    LoomCommitId next_commit_id;
    loom_u8 oam_count;
    loom_u8 dma_count;
    loom_u8 open;
    loom_u8 initialized;
} LoomFrameBuildState;

static LoomFrameBuildState loom_frame_build_state;

/* One copy of the default display costs less than twenty field stores on
 * the 65816. Field order follows LoomDisplayState. */
static const LoomDisplayState loom_frame_build_display_template = {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    LOOM_COLOR_BGR555(0u, 0u, 0u),
    LOOM_COLOR_BGR555(0u, 0u, 0u),
    LOOM_DISPLAY_MODE_1,
    15u,
    0u,
    0u,
    LOOM_OBJ_SIZE_8_16,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u};

static void loom_frame_build_default_display(LoomDisplayState *display)
{
    *display = loom_frame_build_display_template;
}

static LoomCommitId loom_frame_build_next_id(LoomCommitId current)
{
    if (current == (LoomCommitId)(LOOM_COMMIT_NONE - 1u)) {
        return 0u;
    }
    return (LoomCommitId)(current + 1u);
}

LoomStatus loom_frame_build_initialize(void)
{
    /* The guarded runtime root owns one-time initialization. Do not rely on
     * target .bss clearing for private module sentinels. */
    loom_frame_build_state.next_commit_id = 0u;
    loom_frame_build_state.oam_count = 0u;
    loom_frame_build_state.dma_count = 0u;
    loom_frame_build_state.open = LOOM_FALSE;
    loom_frame_build_state.initialized = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_begin(void)
{
    if (loom_frame_build_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_frame_build_state.open != LOOM_FALSE) {
        return LOOM_STATUS_BUSY;
    }
    loom_frame_build_state.commit.commit_id =
        loom_frame_build_state.next_commit_id;
    loom_frame_build_default_display(&loom_frame_build_state.commit.display);
    loom_frame_build_state.commit.raster.program =
        LOOM_RASTER_PROGRAM_NONE;
    loom_frame_build_state.commit.raster.state =
        LOOM_RASTER_STATE_NONE;
    loom_frame_build_state.commit.oam_count = 0u;
    loom_frame_build_state.commit.dma_count = 0u;
    loom_frame_build_state.commit.flags =
        LOOM_FRAME_COMMIT_FLAGS_NONE;
    loom_frame_build_state.commit.reserved = 0u;
    loom_frame_build_state.oam_count = 0u;
    loom_frame_build_state.dma_count = 0u;
    loom_frame_build_state.next_dma = loom_frame_build_state.dma;
    loom_frame_build_state.open = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_set_display(const LoomDisplayState *display)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (display == (const LoomDisplayState *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_frame_build_state.commit.display = *display;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_set_raster(const LoomRasterBinding *raster)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (raster == (const LoomRasterBinding *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_frame_build_state.commit.raster = *raster;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_add_oam(const LoomOamEntry *entry)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (entry == (const LoomOamEntry *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_frame_build_state.oam_count >= LOOM_FRAME_OAM_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    loom_frame_build_state.oam[loom_frame_build_state.oam_count] = *entry;
    ++loom_frame_build_state.oam_count;
    return LOOM_STATUS_OK;
}

LoomDisplayState *loom_frame_build_display(void)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return (LoomDisplayState *)0;
    }
    return &loom_frame_build_state.commit.display;
}

LoomRasterBinding *loom_frame_build_raster(void)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return (LoomRasterBinding *)0;
    }
    return &loom_frame_build_state.commit.raster;
}

LoomOamEntry *loom_frame_build_reserve_oam(void)
{
    LoomOamEntry *entry;

    if (loom_frame_build_state.open == LOOM_FALSE ||
        loom_frame_build_state.oam_count >= LOOM_FRAME_OAM_CAPACITY) {
        return (LoomOamEntry *)0;
    }
    entry = &loom_frame_build_state.oam[loom_frame_build_state.oam_count];
    ++loom_frame_build_state.oam_count;
    return entry;
}

LoomDmaJob *loom_frame_build_reserve_dma(void)
{
    LoomDmaJob *job;

    if (loom_frame_build_state.open == LOOM_FALSE ||
        loom_frame_build_state.dma_count >= LOOM_FRAME_DMA_CAPACITY) {
        return (LoomDmaJob *)0;
    }
    job = loom_frame_build_state.next_dma;
    ++loom_frame_build_state.next_dma;
    ++loom_frame_build_state.dma_count;
    return job;
}

LoomDmaJob *loom_frame_build_dma_space(loom_u8 *room)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        *room = 0u;
        return (LoomDmaJob *)0;
    }
    *room = (loom_u8)(LOOM_FRAME_DMA_CAPACITY - loom_frame_build_state.dma_count);
    return loom_frame_build_state.next_dma;
}

void loom_frame_build_dma_advance(loom_u8 count)
{
    loom_frame_build_state.next_dma += count;
    loom_frame_build_state.dma_count = (loom_u8)(loom_frame_build_state.dma_count + count);
}

loom_u16 loom_frame_build_dma_bytes(void)
{
    const LoomDmaJob *job;
    loom_u16 bytes;
    loom_u8 remaining;

    bytes = 0u;
    job = loom_frame_build_state.dma;
    for (remaining = loom_frame_build_state.dma_count; remaining != 0u;
         --remaining, ++job) {
        bytes = (loom_u16)(bytes + job->byte_count);
    }
    return bytes;
}

LoomStatus loom_frame_build_add_dma(const LoomDmaJob *job)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (job == (const LoomDmaJob *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_frame_build_state.dma_count >= LOOM_FRAME_DMA_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    /* One pointer, not an indexed store: subscripts multiply on 816-tcc. */
    *loom_frame_build_state.next_dma = *job;
    ++loom_frame_build_state.next_dma;
    ++loom_frame_build_state.dma_count;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_current_commit_id(LoomCommitId *commit_id)
{
    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (commit_id == (LoomCommitId *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    *commit_id = loom_frame_build_state.commit.commit_id;
    return LOOM_STATUS_OK;
}

LoomStatus loom_frame_build_submit(void)
{
    const LoomDmaJob *job;
    LoomStatus status;
    loom_u8 index;

    if (loom_frame_build_state.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    loom_frame_build_state.commit.oam_count =
        loom_frame_build_state.oam_count;
    loom_frame_build_state.commit.dma_count =
        loom_frame_build_state.dma_count;
    status = loom_port_frame_begin(&loom_frame_build_state.commit);
    if (status != LOOM_STATUS_OK) {
        /* The port may already hold staging from this build (the console's
         * Mode 1 batch writes sprites before the commit opens), so a refused
         * commit is abandoned there too; a busy port at a room swap otherwise
         * left the next commit's sprites refused as duplicates (GAME-001). */
        loom_port_frame_abort();
        loom_frame_build_state.open = LOOM_FALSE;
        return status;
    }
    LOOM_RUNTIME_TRACE_MARK(4u);
    for (index = 0u; index < loom_frame_build_state.oam_count; ++index) {
        status = loom_port_oam_stage(&loom_frame_build_state.oam[index]);
        if (status != LOOM_STATUS_OK) {
            loom_port_frame_abort();
            loom_frame_build_state.open = LOOM_FALSE;
            return status;
        }
    }
    LOOM_RUNTIME_TRACE_MARK(5u);
    job = loom_frame_build_state.dma;
    for (index = 0u; index < loom_frame_build_state.dma_count; ++index, ++job) {
        status = loom_port_dma_stage(job);
        if (status != LOOM_STATUS_OK) {
            loom_port_frame_abort();
            loom_frame_build_state.open = LOOM_FALSE;
            return status;
        }
    }
    LOOM_RUNTIME_TRACE_MARK(6u);
    status = loom_port_frame_commit();
    if (status != LOOM_STATUS_OK) {
        loom_port_frame_abort();
        loom_frame_build_state.open = LOOM_FALSE;
        return status;
    }
    LOOM_RUNTIME_TRACE_MARK(7u);
    loom_frame_build_state.open = LOOM_FALSE;
    loom_frame_build_state.next_commit_id = loom_frame_build_next_id(
        loom_frame_build_state.next_commit_id);
    return LOOM_STATUS_OK;
}

void loom_frame_build_abort(void)
{
    loom_frame_build_state.open = LOOM_FALSE;
    loom_frame_build_state.oam_count = 0u;
    loom_frame_build_state.dma_count = 0u;
    loom_frame_build_state.next_dma = loom_frame_build_state.dma;
}
