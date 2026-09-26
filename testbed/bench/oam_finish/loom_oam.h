/* Loom 2e278a7^: runtime/include/loom/types.h and video.h (LoomOamEntry and
 * the OAM limits), plus the OAM state oam.asm keeps in RAM (2e278a7). */
#ifndef BENCH_LOOM_OAM_H
#define BENCH_LOOM_OAM_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;
typedef unsigned char u8;
typedef unsigned short u16;
typedef loom_u8 LoomStatus;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_STATUS_OK ((LoomStatus)0u)
#define LOOM_STATUS_CAPACITY ((LoomStatus)2u)
#define LOOM_STATUS_INVALID_ARGUMENT ((LoomStatus)3u)

#define LOOM_OAM_FLAG_FLIP_X ((loom_u8)0x01u)
#define LOOM_OAM_FLAG_FLIP_Y ((loom_u8)0x02u)
#define LOOM_OAM_SIZE_SMALL ((loom_u8)0u)
#define LOOM_OAM_SIZE_LARGE ((loom_u8)1u)
#define LOOM_OAM_SLOT_MAX ((loom_u8)127u)
#define LOOM_OAM_TILE_INDEX_MAX ((loom_u16)511u)
#define LOOM_OAM_X_MIN ((loom_s16)-256)
#define LOOM_OAM_X_MAX ((loom_s16)255)
#define LOOM_OAM_Y_MIN ((loom_s16)-64)
#define LOOM_OAM_Y_MAX ((loom_s16)255)

#define LOOM_FRAME_OAM_CAPACITY ((loom_u8)33u)

typedef struct LoomOamEntry {
    loom_s16 x;
    loom_s16 y;
    loom_u16 tile_index;
    loom_u8 slot;
    loom_u8 palette;
    loom_u8 priority;
    loom_u8 size;
    loom_u8 flags;
    loom_u8 reserved;
} LoomOamEntry;

/* oam.asm reads the entry by offset: x @0, y @2, tile @4, slot @6 .. @11. */
typedef char loom_oam_entry_is_twelve_bytes
    [(sizeof(LoomOamEntry) == 12u) ? 1 : -1];

/* pvsneslib's OAM shadow: 512 bytes of low table, 32 of high table (on the
 * console it comes from pvsneslib's libc.obj; the host driver defines it). */
extern u8 oamMemory[];

/* oam.asm's RAM (the C unit defines the same names). */
extern loom_u8 loom_pvs_oam_generation;
extern loom_u8 loom_pvs_oam_count;
extern loom_u8 loom_pvs_oam_previous_count;
extern loom_u8 loom_pvs_oam_mark[128];
extern loom_u8 loom_pvs_oam_current[34];
extern loom_u8 loom_pvs_oam_previous[34];

LoomStatus loom_pvs_oam_stage(const LoomOamEntry *entry);
void loom_pvs_oam_finish(void);

#endif
