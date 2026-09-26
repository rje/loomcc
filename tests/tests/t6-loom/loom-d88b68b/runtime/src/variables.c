#include <loom/variables.h>

static loom_u16 *loom_variable_block(loom_u8 lifetime, loom_u16 *count)
{
    if (lifetime == LOOM_VARIABLE_LIFETIME_PERSISTENT) {
        *count = loom_generated_variable_persistent_count;
        return loom_generated_variable_persistent;
    }
    if (lifetime == LOOM_VARIABLE_LIFETIME_SCENE) {
        *count = loom_generated_variable_scene_count;
        return loom_generated_variable_scene;
    }
    if (lifetime == LOOM_VARIABLE_LIFETIME_FRAME) {
        *count = loom_generated_variable_frame_count;
        return loom_generated_variable_frame;
    }
    *count = 0u;
    return (loom_u16 *)0;
}

static const loom_u16 *loom_variable_defaults(loom_u8 lifetime)
{
    if (lifetime == LOOM_VARIABLE_LIFETIME_PERSISTENT) {
        return loom_generated_variable_persistent_defaults;
    }
    if (lifetime == LOOM_VARIABLE_LIFETIME_SCENE) {
        return loom_generated_variable_scene_defaults;
    }
    return loom_generated_variable_frame_defaults;
}

static const LoomVariableRecord *loom_variable_record(LoomVariableHandle variable)
{
    if (variable >= loom_generated_variable_count) {
        return (const LoomVariableRecord *)0;
    }
    return &loom_generated_variables[variable];
}

static loom_u16 *loom_variable_words(const LoomVariableRecord *record)
{
    loom_u16 count;
    loom_u16 *block = loom_variable_block(record->lifetime, &count);

    if (block == (loom_u16 *)0 ||
        (loom_u16)(record->storage_index + record->word_count) > count) {
        return (loom_u16 *)0;
    }
    return block + record->storage_index;
}

LoomStatus loom_variable_reset_lifetime(loom_u8 lifetime)
{
    loom_u16 count;
    loom_u16 index;
    loom_u16 *block = loom_variable_block(lifetime, &count);
    const loom_u16 *defaults = loom_variable_defaults(lifetime);

    if (block == (loom_u16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        block[index] = defaults[index];
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_variable_initialize(void)
{
    LoomStatus status;

    status = loom_variable_reset_lifetime(LOOM_VARIABLE_LIFETIME_PERSISTENT);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    status = loom_variable_reset_lifetime(LOOM_VARIABLE_LIFETIME_SCENE);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    return loom_variable_reset_lifetime(LOOM_VARIABLE_LIFETIME_FRAME);
}

/* Game hooks read variables every tick, so this is one function: the record
 * by a shift (a struct subscript is a multiply helper on 816-tcc) and the
 * lifetime's block chosen inline rather than through three calls. */
loom_u16 loom_variable_get(LoomVariableHandle variable)
{
    const LoomVariableRecord *record;
    const loom_u16 *block;
    loom_u16 count;
    loom_u16 index;

    if (variable >= loom_generated_variable_count) {
        return 0u;
    }
    record = (const LoomVariableRecord *)((const loom_u8 *)loom_generated_variables +
                                          ((loom_u16)variable << 3));
    if (record->kind == LOOM_VARIABLE_KIND_ADVENTURE_BOOL) {
        return loom_generated_variable_external_get(record->storage_index);
    }
    if (record->lifetime == LOOM_VARIABLE_LIFETIME_PERSISTENT) {
        block = loom_generated_variable_persistent;
        count = loom_generated_variable_persistent_count;
    } else if (record->lifetime == LOOM_VARIABLE_LIFETIME_SCENE) {
        block = loom_generated_variable_scene;
        count = loom_generated_variable_scene_count;
    } else if (record->lifetime == LOOM_VARIABLE_LIFETIME_FRAME) {
        block = loom_generated_variable_frame;
        count = loom_generated_variable_frame_count;
    } else {
        return 0u;
    }
    index = record->storage_index;
    if ((loom_u16)(index + record->word_count) > count) {
        return 0u;
    }
    return *(const loom_u16 *)((const loom_u8 *)block + (index << 1));
}

LoomStatus loom_variable_set(LoomVariableHandle variable, loom_u16 value)
{
    const LoomVariableRecord *record = loom_variable_record(variable);
    loom_u16 *words;

    if (record == (const LoomVariableRecord *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (record->kind == LOOM_VARIABLE_KIND_ADVENTURE_BOOL) {
        return loom_generated_variable_external_set(record->storage_index,
                                                    value != 0u ? 1u : 0u);
    }
    if (record->kind == LOOM_VARIABLE_KIND_FLAGS) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (record->kind == LOOM_VARIABLE_KIND_BOOL) {
        value = value != 0u ? 1u : 0u;
    } else if (record->limit != 0u && value >= record->limit) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    words = loom_variable_words(record);
    if (words == (loom_u16 *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    words[0] = value;
    return LOOM_STATUS_OK;
}

loom_u8 loom_variable_flag_test(LoomVariableHandle variable, loom_u8 bit)
{
    const LoomVariableRecord *record = loom_variable_record(variable);
    const loom_u16 *words;

    if (record == (const LoomVariableRecord *)0 ||
        record->kind != LOOM_VARIABLE_KIND_FLAGS || bit >= record->limit) {
        return LOOM_FALSE;
    }
    words = loom_variable_words(record);
    if (words == (const loom_u16 *)0) {
        return LOOM_FALSE;
    }
    return (words[bit >> 4] & (loom_u16)(1u << (bit & 15u))) != 0u
               ? LOOM_TRUE
               : LOOM_FALSE;
}

LoomStatus loom_variable_flag_write(LoomVariableHandle variable,
                                    loom_u8 bit,
                                    loom_u8 value)
{
    const LoomVariableRecord *record = loom_variable_record(variable);
    loom_u16 *words;
    loom_u16 mask;

    if (record == (const LoomVariableRecord *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (record->kind != LOOM_VARIABLE_KIND_FLAGS || bit >= record->limit) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    words = loom_variable_words(record);
    if (words == (loom_u16 *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    mask = (loom_u16)(1u << (bit & 15u));
    if (value != LOOM_FALSE) {
        words[bit >> 4] |= mask;
    } else {
        words[bit >> 4] &= (loom_u16)~mask;
    }
    return LOOM_STATUS_OK;
}

loom_u16 loom_variable_word(LoomVariableHandle variable, loom_u8 word)
{
    const LoomVariableRecord *record = loom_variable_record(variable);
    const loom_u16 *words;

    if (record == (const LoomVariableRecord *)0 || word >= record->word_count) {
        return 0u;
    }
    if (record->kind == LOOM_VARIABLE_KIND_ADVENTURE_BOOL) {
        return loom_generated_variable_external_get(record->storage_index);
    }
    words = loom_variable_words(record);
    return words == (const loom_u16 *)0 ? 0u : words[word];
}

loom_u16 loom_variable_count(void)
{
    return loom_generated_variable_count;
}
