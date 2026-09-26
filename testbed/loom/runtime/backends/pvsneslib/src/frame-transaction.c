#include <loom-pvsneslib/frame-transaction.h>



/* Exported: the adapter reads it through the macros in the header on the
 * console, where each accessor call cost 816-tcc about twenty instructions. */
LoomPvsFrameTransaction loom_pvs_frame_transaction;

void loom_pvs_frame_transaction_initialize(void)
{
    loom_u8 slot;

    loom_pvs_frame_transaction.dma_bytes = 0u;
    loom_pvs_frame_transaction.oam_count = 0u;
    loom_pvs_frame_transaction.dma_count = 0u;
    loom_pvs_frame_transaction.open = LOOM_FALSE;
    loom_pvs_frame_transaction.commit = (const LoomFrameCommit *)0;
    loom_pvs_frame_transaction.initialized = LOOM_TRUE;
    loom_pvs_frame_transaction.ready = LOOM_FALSE;
    loom_pvs_frame_transaction.generation = 0u;
    for (slot = 0u; slot < LOOM_OAM_SLOT_MAX; ++slot) {
        loom_pvs_frame_transaction.slot_mark[slot] = 0xffu;
    }
    loom_pvs_frame_transaction.slot_mark[LOOM_OAM_SLOT_MAX] = 0xffu;
}

#if !defined(__65816__)
loom_u8 loom_pvs_frame_transaction_ready(void)
{
    return loom_pvs_frame_transaction.ready;
}

const LoomFrameCommit *loom_pvs_frame_transaction_commit(void)
{
    return loom_pvs_frame_transaction.commit;
}

const LoomDmaJob *loom_pvs_frame_transaction_dma(void)
{
    return loom_pvs_frame_transaction.dma;
}

loom_u8 loom_pvs_frame_transaction_dma_count(void)
{
    return loom_pvs_frame_transaction.dma_count;
}

void loom_pvs_frame_transaction_presented(void)
{
    loom_pvs_frame_transaction.ready = LOOM_FALSE;
}
#endif

LoomStatus loom_port_frame_begin(const LoomFrameCommit *commit)
{
    if (loom_pvs_frame_transaction.open != LOOM_FALSE ||
        loom_pvs_frame_transaction.ready != LOOM_FALSE) {
        return LOOM_STATUS_BUSY;
    }
#if !defined(__65816__)
    /* The console's only caller is loom_frame_build_submit, whose commit is
     * never null, is numbered, carries no flags and stays within the
     * capacities it enforces as it is built; a transaction that was never
     * initialized is never open. Each of these compares is a dozen
     * instructions on 816-tcc every tick, so the host keeps them for the
     * contract tests and for custom builders. */
    if (loom_pvs_frame_transaction.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (commit == (const LoomFrameCommit *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (commit->commit_id == LOOM_COMMIT_NONE ||
        commit->flags != LOOM_FRAME_COMMIT_FLAGS_NONE ||
        commit->reserved != 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (commit->oam_count > LOOM_FRAME_OAM_CAPACITY ||
        commit->dma_count > LOOM_FRAME_DMA_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
#endif
#if !defined(__65816__)
    /* The console trusts the display state its own Mode 1 build wrote from
     * plan-validated data; the host keeps the check for the contract tests
     * and for custom builders. Eighteen field compares cost 816-tcc ten
     * scanlines a frame. */
    if (loom_pvs_target_commit_supported(commit) == LOOM_FALSE) {
        return LOOM_STATUS_UNSUPPORTED;
    }
#endif
    loom_pvs_frame_transaction.commit = commit;
    loom_pvs_frame_transaction.dma_bytes = 0u;
    loom_pvs_frame_transaction.oam_count = 0u;
    loom_pvs_frame_transaction.next_oam = loom_pvs_frame_transaction.oam;
    loom_pvs_frame_transaction.next_dma = loom_pvs_frame_transaction.dma;
    loom_pvs_frame_transaction.dma_count = 0u;
    loom_pvs_frame_transaction.open = LOOM_TRUE;
#if defined(__65816__)
    loom_pvs_oam_begin();
#else
    /* 0xff never marks a slot, so every generation stays distinct. */
    loom_pvs_frame_transaction.generation =
        (loom_u8)(loom_pvs_frame_transaction.generation == 0xfeu
                      ? 0u
                      : loom_pvs_frame_transaction.generation + 1u);
#endif
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_oam_stage(const LoomOamEntry *entry)
{
    loom_u8 slot;

    if (loom_pvs_frame_transaction.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (entry == (const LoomOamEntry *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_frame_transaction.oam_count >=
        loom_pvs_frame_transaction.commit->oam_count) {
        return LOOM_STATUS_CAPACITY;
    }
#if defined(__65816__)
    {
        LoomStatus status;

        status = loom_pvs_oam_stage(entry);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_pvs_frame_transaction.oam_count;
        return LOOM_STATUS_OK;
    }
#else
    /* One OR of every out-of-range bit and one unsigned window test per
     * coordinate: each separate comparison costs 816-tcc a pointer reload
     * and a signed compare sequence, and this runs once per sprite. */
    slot = entry->slot;
    if (((loom_u8)(slot & (loom_u8)(~LOOM_OAM_SLOT_MAX)) |
         (loom_u8)(entry->palette & (loom_u8)(~7u)) |
         (loom_u8)(entry->priority & (loom_u8)(~3u)) |
         (loom_u8)(entry->size & (loom_u8)(~LOOM_OAM_SIZE_LARGE)) |
         (loom_u8)(entry->flags &
                   (loom_u8)(~(LOOM_OAM_FLAG_FLIP_X |
                               LOOM_OAM_FLAG_FLIP_Y))) |
         entry->reserved) != 0u ||
        entry->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
        (loom_u16)(entry->x - LOOM_OAM_X_MIN) >
            (loom_u16)(LOOM_OAM_X_MAX - LOOM_OAM_X_MIN) ||
        (loom_u16)(entry->y - LOOM_OAM_Y_MIN) >
            (loom_u16)(LOOM_OAM_Y_MAX - LOOM_OAM_Y_MIN)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_frame_transaction.slot_mark[slot] ==
        loom_pvs_frame_transaction.generation) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_pvs_frame_transaction.slot_mark[slot] =
        loom_pvs_frame_transaction.generation;
    *loom_pvs_frame_transaction.next_oam = *entry;
    ++loom_pvs_frame_transaction.next_oam;
    ++loom_pvs_frame_transaction.oam_count;
    return LOOM_STATUS_OK;
#endif
}

LoomStatus loom_port_dma_stage(const LoomDmaJob *job)
{
    const LoomDmaJob *staged;
    loom_u8 index;

    if (loom_pvs_frame_transaction.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (job == (const LoomDmaJob *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_frame_transaction.dma_count >=
        loom_pvs_frame_transaction.commit->dma_count) {
        return LOOM_STATUS_CAPACITY;
    }
#if defined(__65816__) && !defined(LOOM_BUILD_DEBUG)
    /* A release cartridge stages jobs Loom's own runtime built, which the
     * debug build and the host suites validate: the checks and the
     * duplicate scan cost the frame about a hundred instructions a job. */
    *loom_pvs_frame_transaction.next_dma = *job;
    ++loom_pvs_frame_transaction.next_dma;
    ++loom_pvs_frame_transaction.dma_count;
    loom_pvs_frame_transaction.dma_bytes =
        (loom_u16)(loom_pvs_frame_transaction.dma_bytes + job->byte_count);
    (void)staged;
    (void)index;
    return LOOM_STATUS_OK;
#endif
    if (job->policy != LOOM_DMA_REQUIRED) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (job->byte_count == 0u || (job->byte_count & 1u) != 0u ||
        (job->destination_offset & 1u) != 0u || job->reserved != 0u ||
        job->source_kind > LOOM_DMA_SOURCE_WRAM_BLOCK ||
        job->destination_kind > LOOM_DMA_DESTINATION_VRAM_COLUMN) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
#if !defined(__65816__)
    /* Source spans and destination windows come from plan-validated data on
     * the console; the host checks them for the contract tests. */
    if (loom_pvs_target_dma_source_valid(job) == LOOM_FALSE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if ((job->destination_kind == LOOM_DMA_DESTINATION_VRAM &&
         job->destination_offset >
             (loom_u16)(0xffffu - (job->byte_count - 1u))) ||
        (job->destination_kind == LOOM_DMA_DESTINATION_VRAM_COLUMN &&
         (job->byte_count > 64u ||
          job->destination_offset >
              (loom_u16)(0xffffu -
                         ((loom_u16)(job->byte_count / 2u - 1u) * 64u +
                          1u)))) ||
        (job->destination_kind == LOOM_DMA_DESTINATION_CGRAM &&
         (job->byte_count > 512u ||
          job->destination_offset >
              (loom_u16)(511u - (job->byte_count - 1u))))) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
#endif
    if (job->byte_count > LOOM_FRAME_REQUIRED_DMA_BYTES_MAX ||
        loom_pvs_frame_transaction.dma_bytes >
        (loom_u16)(LOOM_FRAME_REQUIRED_DMA_BYTES_MAX -
                   job->byte_count)) {
        return LOOM_STATUS_CAPACITY;
    }
    staged = loom_pvs_frame_transaction.dma;
    for (index = 0u; index < loom_pvs_frame_transaction.dma_count;
         ++index, ++staged) {
        if (staged->job_id == job->job_id) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    *loom_pvs_frame_transaction.next_dma = *job;
    ++loom_pvs_frame_transaction.next_dma;
    ++loom_pvs_frame_transaction.dma_count;
    loom_pvs_frame_transaction.dma_bytes =
        (loom_u16)(loom_pvs_frame_transaction.dma_bytes +
                   job->byte_count);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_frame_commit(void)
{
#if !defined(__65816__)
    LoomStatus status;
#endif

    if (loom_pvs_frame_transaction.open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
#if !defined(__65816__)
    /* On the console submit stages exactly the counts it opened with. */
    if (loom_pvs_frame_transaction.oam_count !=
            loom_pvs_frame_transaction.commit->oam_count ||
        loom_pvs_frame_transaction.dma_count !=
            loom_pvs_frame_transaction.commit->dma_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
#endif
#if defined(__65816__)
    loom_pvs_oam_finish();
#else
    status = loom_pvs_target_prepare_oam(
        loom_pvs_frame_transaction.oam,
        loom_pvs_frame_transaction.oam_count);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif
    loom_pvs_frame_transaction.open = LOOM_FALSE;
    /* Publish last: NMI can observe only the complete immutable transaction. */
    loom_pvs_frame_transaction.ready = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

void loom_port_frame_abort(void)
{
#if defined(__65816__)
    loom_pvs_oam_abort();
#endif
    if (loom_pvs_frame_transaction.open != LOOM_FALSE) {
        loom_pvs_frame_transaction.open = LOOM_FALSE;
        loom_pvs_frame_transaction.dma_bytes = 0u;
        loom_pvs_frame_transaction.oam_count = 0u;
        loom_pvs_frame_transaction.dma_count = 0u;
    }
}
