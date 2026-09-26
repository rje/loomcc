#include <loom/port.h>
#include <loom/save.h>
#include <loom/variables.h>

typedef struct LoomSaveState {
    loom_u8 slots;
    loom_u8 last_status;
    loom_u8 initialized;
    loom_u8 reserved;
    /* One slot's image, header first; word-aligned so the payload is read
     * and summed as words. */
    loom_u16 image[LOOM_SAVE_SLOT_BYTES / 2u];
} LoomSaveState;

static LoomSaveState loom_save_state;

loom_u8 loom_save_debug_epoch;

static loom_u16 loom_save_payload_words(void)
{
    return (loom_u16)(LOOM_SAVE_FIXED_WORDS +
                      loom_generated_variable_persistent_count);
}

static loom_u16 loom_save_checksum(const loom_u16 *payload, loom_u16 words)
{
    loom_u16 sum;
    loom_u16 index;

    sum = LOOM_SAVE_MAGIC;
    for (index = 0u; index < words; ++index) {
        sum = (loom_u16)(sum + payload[index]);
    }
    return (loom_u16)(0xffffu - sum);
}

static loom_u8 loom_save_slot_valid(loom_u8 slot)
{
    loom_u16 end;

    if (slot >= LOOM_SAVE_SLOT_COUNT) {
        return LOOM_FALSE;
    }
    end = (loom_u16)((loom_u16)(slot + 1u) * LOOM_SAVE_SLOT_BYTES);
    return end <= loom_generated_save_sram_bytes ? LOOM_TRUE : LOOM_FALSE;
}

static LoomStatus loom_save_load_image(loom_u8 slot)
{
    return loom_port_sram_read((loom_u16)(slot * LOOM_SAVE_SLOT_BYTES),
                               (loom_u8 *)loom_save_state.image,
                               LOOM_SAVE_SLOT_BYTES);
}

/* Reads the slot into the image and checks every header field against the
 * payload; a slot that fails is treated as empty. */
static LoomStatus loom_save_validate_slot(loom_u8 slot)
{
    LoomStatus status;
    loom_u16 words;
    loom_u16 *header;

    status = loom_save_load_image(slot);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    header = loom_save_state.image;
    words = loom_save_payload_words();
    if (header[0] != LOOM_SAVE_MAGIC ||
        header[1] != loom_generated_save_layout ||
        header[2] != (loom_u16)(words * 2u) ||
        header[3] != loom_save_checksum(&header[4], words)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_OK;
}

static void loom_save_note(loom_u8 slot, LoomStatus status, loom_u8 present)
{
    loom_u8 bit;

    bit = (loom_u8)(1u << slot);
    if (present != LOOM_FALSE) {
        loom_save_state.slots = (loom_u8)(loom_save_state.slots | bit);
    } else {
        loom_save_state.slots = (loom_u8)(loom_save_state.slots & (loom_u8)~bit);
    }
    loom_save_state.last_status = (loom_u8)status;
    ++loom_save_debug_epoch;
}

LoomStatus loom_save_initialize(void)
{
    loom_u8 slot;

    loom_save_state.slots = 0u;
    loom_save_state.last_status = (loom_u8)LOOM_STATUS_OK;
    loom_save_state.initialized = LOOM_TRUE;
    ++loom_save_debug_epoch;
    if (loom_generated_save_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    if (loom_save_payload_words() > LOOM_SAVE_PAYLOAD_WORDS_MAX) {
        loom_save_state.initialized = LOOM_FALSE;
        return LOOM_STATUS_CAPACITY;
    }
    for (slot = 0u; slot < LOOM_SAVE_SLOT_COUNT; ++slot) {
        if (loom_save_slot_valid(slot) != LOOM_FALSE &&
            loom_save_validate_slot(slot) == LOOM_STATUS_OK) {
            loom_save_state.slots = (loom_u8)(loom_save_state.slots | (loom_u8)(1u << slot));
        }
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_save_check(loom_u8 slot)
{
    if (loom_save_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_save_enabled == LOOM_FALSE) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (loom_save_slot_valid(slot) == LOOM_FALSE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_save_write(loom_u8 slot)
{
    LoomStatus status;
    loom_u16 words;
    loom_u16 index;
    loom_u16 *header;
    loom_u16 *payload;
    loom_s16 x;
    loom_s16 y;

    status = loom_save_check(slot);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    header = loom_save_state.image;
    payload = &loom_save_state.image[LOOM_SAVE_HEADER_BYTES / 2u];
    words = loom_save_payload_words();
    x = 0;
    y = 0;
    loom_generated_save_player_position(&x, &y);
    payload[0] = loom_generated_save_scene_index();
    payload[1] = (loom_u16)x;
    payload[2] = (loom_u16)y;
    payload[3] = loom_generated_save_external_word();
    for (index = 0u; index < loom_generated_variable_persistent_count; ++index) {
        payload[LOOM_SAVE_FIXED_WORDS + index] =
            loom_generated_variable_persistent[index];
    }
    header[0] = LOOM_SAVE_MAGIC;
    header[1] = loom_generated_save_layout;
    header[2] = (loom_u16)(words * 2u);
    header[3] = loom_save_checksum(payload, words);
    status = loom_port_sram_write((loom_u16)(slot * LOOM_SAVE_SLOT_BYTES),
                                  (const loom_u8 *)loom_save_state.image,
                                  (loom_u16)(LOOM_SAVE_HEADER_BYTES + words * 2u));
    loom_save_note(slot, status, status == LOOM_STATUS_OK ? LOOM_TRUE : LOOM_FALSE);
    return status;
}

LoomStatus loom_save_read(loom_u8 slot)
{
    LoomStatus status;
    loom_u16 index;
    const loom_u16 *payload;

    status = loom_save_check(slot);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    status = loom_save_validate_slot(slot);
    if (status != LOOM_STATUS_OK) {
        /* Nothing was touched: the game keeps running as it was. */
        loom_save_note(slot, status, LOOM_FALSE);
        return status;
    }
    payload = &loom_save_state.image[LOOM_SAVE_HEADER_BYTES / 2u];
    for (index = 0u; index < loom_generated_variable_persistent_count; ++index) {
        loom_generated_variable_persistent[index] =
            payload[LOOM_SAVE_FIXED_WORDS + index];
    }
    status = loom_generated_save_restore_external(payload[3]);
    if (status == LOOM_STATUS_OK) {
        status = loom_generated_save_restore(payload[0],
                                             (loom_s16)payload[1],
                                             (loom_s16)payload[2]);
    }
    loom_save_note(slot, status, LOOM_TRUE);
    return status;
}

loom_u8 loom_save_exists(loom_u8 slot)
{
    if (slot >= LOOM_SAVE_SLOT_COUNT) {
        return LOOM_FALSE;
    }
    return (loom_save_state.slots & (loom_u8)(1u << slot)) != 0u ? LOOM_TRUE
                                                                  : LOOM_FALSE;
}

LoomStatus loom_save_erase(loom_u8 slot)
{
    LoomStatus status;
    loom_u16 index;

    status = loom_save_check(slot);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    for (index = 0u; index < LOOM_SAVE_HEADER_BYTES / 2u; ++index) {
        loom_save_state.image[index] = 0u;
    }
    status = loom_port_sram_write((loom_u16)(slot * LOOM_SAVE_SLOT_BYTES),
                                  (const loom_u8 *)loom_save_state.image,
                                  LOOM_SAVE_HEADER_BYTES);
    loom_save_note(slot, status, LOOM_FALSE);
    return status;
}

loom_u8 loom_save_slots(void)
{
    return loom_save_state.slots;
}

loom_u8 loom_save_last_status(void)
{
    return loom_save_state.last_status;
}
