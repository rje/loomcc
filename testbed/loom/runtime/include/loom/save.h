#ifndef LOOM_SAVE_H
#define LOOM_SAVE_H

#include <loom/types.h>

/*
 * SRAM save slots: the persistent variable block plus the scene and the
 * player's position, in a fixed-stride slot with a header the reader checks
 * before touching the game. Three slots; a slot is 256 bytes whatever the
 * project stores, so a save made by one build of a project reads back in
 * another as long as the layout word agrees.
 */
#define LOOM_SAVE_SLOT_COUNT ((loom_u8)3u)
#define LOOM_SAVE_SLOT_BYTES ((loom_u16)256u)
#define LOOM_SAVE_HEADER_BYTES ((loom_u16)8u)
#define LOOM_SAVE_MAGIC ((loom_u16)0x534cu)
/* Scene index, spawn X, spawn Y and the external word (the adventure
 * kit's flags, when a project has them) precede the variables in a payload. */
#define LOOM_SAVE_FIXED_WORDS ((loom_u16)4u)
#define LOOM_SAVE_PAYLOAD_WORDS_MAX \
    ((loom_u16)((LOOM_SAVE_SLOT_BYTES - LOOM_SAVE_HEADER_BYTES) / 2u))
#define LOOM_SAVE_VARIABLE_WORDS_MAX \
    ((loom_u16)(LOOM_SAVE_PAYLOAD_WORDS_MAX - LOOM_SAVE_FIXED_WORDS))

/* Defined by generated project source. `layout` hashes the persistent
 * variables' names and kinds in storage order and the scene ids, so a build
 * whose saves mean something else fails the read instead of loading them. */
extern const loom_u8 loom_generated_save_enabled;
extern const loom_u16 loom_generated_save_layout;
extern const loom_u16 loom_generated_save_sram_bytes;

/* Bridges the generated schedule implements from the modules it links, so
 * this module never names scene, movement or combat itself. */
loom_u16 loom_generated_save_scene_index(void);
void loom_generated_save_player_position(loom_s16 *x, loom_s16 *y);
LoomStatus loom_generated_save_restore(loom_u16 scene_index,
                                       loom_s16 spawn_x,
                                       loom_s16 spawn_y);
loom_u16 loom_generated_save_external_word(void);
LoomStatus loom_generated_save_restore_external(loom_u16 word);

LoomStatus loom_save_initialize(void);
LoomStatus loom_save_write(loom_u8 slot);
LoomStatus loom_save_read(loom_u8 slot);
loom_u8 loom_save_exists(loom_u8 slot);
LoomStatus loom_save_erase(loom_u8 slot);
/* Bit n set when slot n holds a readable save. */
loom_u8 loom_save_slots(void);
/* The status of the last write, read or erase, for the debug witness. */
loom_u8 loom_save_last_status(void);
extern loom_u8 loom_save_debug_epoch;

#endif
