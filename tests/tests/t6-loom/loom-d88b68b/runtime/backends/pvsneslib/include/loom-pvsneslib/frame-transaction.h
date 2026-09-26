#ifndef LOOM_PVSNESLIB_FRAME_TRANSACTION_H
#define LOOM_PVSNESLIB_FRAME_TRANSACTION_H

#include <loom/frame.h>

typedef struct LoomPvsFrameTransaction {
    /* The builder's commit, borrowed: it stays untouched until the adapter
     * has presented it, and copying it cost 816-tcc a dozen scanlines. */
    const LoomFrameCommit *commit;
    LoomOamEntry oam[LOOM_FRAME_OAM_CAPACITY];
    LoomDmaJob dma[LOOM_FRAME_DMA_CAPACITY];
    LoomDmaJob *next_dma;
    /* The next free OAM staging entry: a pointer walk is far cheaper than an
     * indexed store on 816-tcc, which multiplies for every struct index. */
    LoomOamEntry *next_oam;
    loom_u16 dma_bytes;
    loom_u8 oam_count;
    loom_u8 dma_count;
    loom_u8 open;
    loom_u8 initialized;
    volatile loom_u8 ready;
    /* Slot ownership per commit: a slot whose mark equals the commit's
     * generation was already staged this commit. */
    loom_u8 generation;
    loom_u8 slot_mark[LOOM_OAM_SLOT_MAX + 1u];
} LoomPvsFrameTransaction;
extern LoomPvsFrameTransaction loom_pvs_frame_transaction;

void loom_pvs_frame_transaction_initialize(void);
#if defined(__65816__)
/* The adapter reads the transaction in place on the console. */
#define loom_pvs_frame_transaction_ready() (loom_pvs_frame_transaction.ready)
#define loom_pvs_frame_transaction_commit() (loom_pvs_frame_transaction.commit)
#define loom_pvs_frame_transaction_dma() \
    ((const LoomDmaJob *)loom_pvs_frame_transaction.dma)
#define loom_pvs_frame_transaction_dma_count() \
    (loom_pvs_frame_transaction.dma_count)
#define loom_pvs_frame_transaction_presented() \
    ((void)(loom_pvs_frame_transaction.ready = LOOM_FALSE))
#else
loom_u8 loom_pvs_frame_transaction_ready(void);
const LoomFrameCommit *loom_pvs_frame_transaction_commit(void);
const LoomDmaJob *loom_pvs_frame_transaction_dma(void);
loom_u8 loom_pvs_frame_transaction_dma_count(void);
void loom_pvs_frame_transaction_presented(void);
#endif

/* Direct selected-target hooks; there is no target operation table. */
loom_u8 loom_pvs_target_commit_supported(const LoomFrameCommit *commit);
loom_u8 loom_pvs_target_dma_source_valid(const LoomDmaJob *job);
LoomStatus loom_pvs_target_prepare_oam(const LoomOamEntry *entries,
                                       loom_u8 entry_count);

#if defined(__65816__)
/* The console stages sprites through oam.asm, which validates each entry,
 * rejects duplicate slots, and writes pvsneslib's OAM shadow directly;
 * loom_pvs_target_prepare_oam is the host's C rendition of the same rules. */
void loom_pvs_oam_init(void);
void loom_pvs_oam_begin(void);
void loom_pvs_oam_abort(void);
LoomStatus loom_pvs_oam_stage(const LoomOamEntry *entry);
void loom_pvs_oam_finish(void);
#endif

#endif
