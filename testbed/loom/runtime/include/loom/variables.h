#ifndef LOOM_VARIABLES_H
#define LOOM_VARIABLES_H

#include <loom/types.h>

/* Typed named game-state variables lowered from Data/variables.loom-vars.json.
 * Storage is one fixed 16-bit word block per lifetime; handles index the
 * generated record table in declaration order. */

#define LOOM_VARIABLE_KIND_BOOL ((loom_u8)0u)
#define LOOM_VARIABLE_KIND_U8 ((loom_u8)1u)
#define LOOM_VARIABLE_KIND_U16 ((loom_u8)2u)
#define LOOM_VARIABLE_KIND_I16 ((loom_u8)3u)
#define LOOM_VARIABLE_KIND_ENUM ((loom_u8)4u)
#define LOOM_VARIABLE_KIND_FLAGS ((loom_u8)5u)
/* A boolean stored in the adventure kit's flag word; storage_index is the flag. */
#define LOOM_VARIABLE_KIND_ADVENTURE_BOOL ((loom_u8)6u)

#define LOOM_VARIABLE_LIFETIME_PERSISTENT ((loom_u8)0u)
#define LOOM_VARIABLE_LIFETIME_SCENE ((loom_u8)1u)
#define LOOM_VARIABLE_LIFETIME_FRAME ((loom_u8)2u)

#define LOOM_VARIABLE_NONE ((LoomVariableHandle)0xffffu)

typedef loom_u16 LoomVariableHandle;

typedef struct LoomVariableRecord {
    loom_u16 storage_index;
    loom_u16 limit;
    loom_u8 kind;
    loom_u8 lifetime;
    loom_u8 word_count;
    loom_u8 reserved;
} LoomVariableRecord;

LOOM_STATIC_ASSERT(loom_variable_record_is_eight_bytes,
                   sizeof(LoomVariableRecord) == 8u);

/* Defined by generated variables_data.c. */
extern const loom_u16 loom_generated_variable_count;
extern const LoomVariableRecord loom_generated_variables[];
extern const loom_u16 loom_generated_variable_persistent_count;
extern const loom_u16 loom_generated_variable_scene_count;
extern const loom_u16 loom_generated_variable_frame_count;
extern loom_u16 loom_generated_variable_persistent[];
extern loom_u16 loom_generated_variable_scene[];
extern loom_u16 loom_generated_variable_frame[];
extern const loom_u16 loom_generated_variable_persistent_defaults[];
extern const loom_u16 loom_generated_variable_scene_defaults[];
extern const loom_u16 loom_generated_variable_frame_defaults[];
/* Bridge for adventure-backed bools; generated so this module never links
 * the adventure kit itself. */
loom_u16 loom_generated_variable_external_get(loom_u16 storage_index);
LoomStatus loom_generated_variable_external_set(loom_u16 storage_index,
                                                loom_u16 value);

LoomStatus loom_variable_initialize(void);
LoomStatus loom_variable_reset_lifetime(loom_u8 lifetime);
loom_u16 loom_variable_get(LoomVariableHandle variable);
LoomStatus loom_variable_set(LoomVariableHandle variable, loom_u16 value);
loom_u8 loom_variable_flag_test(LoomVariableHandle variable, loom_u8 bit);
LoomStatus loom_variable_flag_write(LoomVariableHandle variable,
                                    loom_u8 bit,
                                    loom_u8 value);
loom_u16 loom_variable_word(LoomVariableHandle variable, loom_u8 word);
loom_u16 loom_variable_count(void);

#endif
