#include <loom/adventure.h>

typedef struct LoomAdventureState {
    loom_u16 flags;
    LoomAdventureRequest requests[LOOM_ADVENTURE_REQUEST_CAPACITY];
    loom_u16 request_count;
    loom_u8 request_head;
    loom_u8 pending_count;
    loom_u8 last_request_kind;
    loom_u8 last_request_id;
    loom_u8 initialized;
} LoomAdventureState;

static LoomAdventureState loom_adventure_state;

static loom_u16 loom_adventure_flag_mask(loom_u8 flag)
{
    return (loom_u16)(1u << flag);
}

static void loom_adventure_increment(loom_u16 *value)
{
    if (*value != 0xffffu) {
        ++*value;
    }
}

loom_u8 loom_adventure_debug_epoch;

static LoomStatus loom_adventure_enqueue(loom_u8 kind, loom_u8 request_id)
{
    loom_u8 index;

    if (loom_adventure_state.pending_count >=
        LOOM_ADVENTURE_REQUEST_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    index = (loom_u8)((loom_adventure_state.request_head +
                       loom_adventure_state.pending_count) &
                      (LOOM_ADVENTURE_REQUEST_CAPACITY - 1u));
    loom_adventure_state.requests[index].kind = kind;
    loom_adventure_state.requests[index].id = request_id;
    ++loom_adventure_state.pending_count;
    loom_adventure_state.last_request_kind = kind;
    loom_adventure_state.last_request_id = request_id;
    loom_adventure_increment(&loom_adventure_state.request_count);
    ++loom_adventure_debug_epoch;
    return LOOM_STATUS_OK;
}

LoomStatus loom_adventure_initialize(void)
{
    loom_u8 index;

    loom_adventure_state.flags = 0u;
    loom_adventure_state.request_count = 0u;
    loom_adventure_state.request_head = 0u;
    loom_adventure_state.pending_count = 0u;
    ++loom_adventure_debug_epoch;
    loom_adventure_state.last_request_kind = 0u;
    loom_adventure_state.last_request_id = LOOM_ADVENTURE_REQUEST_NONE;
    loom_adventure_state.initialized = LOOM_TRUE;
    for (index = 0u; index < LOOM_ADVENTURE_REQUEST_CAPACITY; ++index) {
        loom_adventure_state.requests[index].kind = 0u;
        loom_adventure_state.requests[index].id =
            LOOM_ADVENTURE_REQUEST_NONE;
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_adventure_flag_set(loom_u8 flag)
{
    if (loom_adventure_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_adventure_state.flags |= loom_adventure_flag_mask(flag);
    ++loom_adventure_debug_epoch;
    return LOOM_STATUS_OK;
}

LoomStatus loom_adventure_flag_clear(loom_u8 flag)
{
    if (loom_adventure_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_adventure_state.flags &=
        (loom_u16)(~loom_adventure_flag_mask(flag));
    ++loom_adventure_debug_epoch;
    return LOOM_STATUS_OK;
}

loom_u8 loom_adventure_flag_is_set(loom_u8 flag)
{
    if (loom_adventure_state.initialized == LOOM_FALSE ||
        flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
        return LOOM_FALSE;
    }
    return (loom_u8)((loom_adventure_state.flags &
                      loom_adventure_flag_mask(flag)) != 0u);
}

loom_u8 loom_adventure_gate_allows(loom_u8 flag, loom_u8 required_set)
{
    loom_u8 set;

    if (flag == LOOM_ADVENTURE_FLAG_NONE) {
        return LOOM_TRUE;
    }
    if (required_set > LOOM_TRUE || flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
        return LOOM_FALSE;
    }
    set = loom_adventure_flag_is_set(flag);
    return (loom_u8)(set == required_set);
}

LoomStatus loom_adventure_try_action(loom_u8 action,
                                     loom_u8 flag,
                                     loom_u8 request_id,
                                     const LoomInputSnapshot *input,
                                     loom_u8 entering,
                                     loom_u8 *dispatched)
{
    LoomStatus status;

    if (loom_adventure_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (input == (const LoomInputSnapshot *)0 ||
        dispatched == (loom_u8 *)0 || request_id == LOOM_ADVENTURE_REQUEST_NONE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    *dispatched = LOOM_FALSE;
    if (action == LOOM_ADVENTURE_ACTION_PICKUP) {
        if (flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
            return LOOM_STATUS_OUT_OF_RANGE;
        }
        if (entering == LOOM_FALSE ||
            loom_adventure_flag_is_set(flag) != LOOM_FALSE) {
            return LOOM_STATUS_OK;
        }
        status = loom_adventure_enqueue(LOOM_ADVENTURE_REQUEST_PICKUP,
                                        request_id);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        status = loom_adventure_flag_set(flag);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        *dispatched = LOOM_TRUE;
        return LOOM_STATUS_OK;
    }
    if (action == LOOM_ADVENTURE_ACTION_INTERACTION) {
        if (flag != LOOM_ADVENTURE_FLAG_NONE &&
            flag >= LOOM_ADVENTURE_FLAG_CAPACITY) {
            return LOOM_STATUS_OUT_OF_RANGE;
        }
        if (input->pad_count == 0u ||
            (input->pads[0].pressed & LOOM_BUTTON_A) == 0u ||
            (flag != LOOM_ADVENTURE_FLAG_NONE &&
             loom_adventure_flag_is_set(flag) != LOOM_FALSE)) {
            return LOOM_STATUS_OK;
        }
        status = loom_adventure_enqueue(LOOM_ADVENTURE_REQUEST_DIALOGUE,
                                        request_id);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        if (flag != LOOM_ADVENTURE_FLAG_NONE) {
            status = loom_adventure_flag_set(flag);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        *dispatched = LOOM_TRUE;
        return LOOM_STATUS_OK;
    }
    return LOOM_STATUS_INVALID_ARGUMENT;
}

LoomStatus loom_adventure_request_pickup(loom_u8 request_id)
{
    if (loom_adventure_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (request_id == LOOM_ADVENTURE_REQUEST_NONE) {
        return LOOM_STATUS_OK;
    }
    return loom_adventure_enqueue(LOOM_ADVENTURE_REQUEST_PICKUP, request_id);
}

LoomStatus loom_adventure_take_request(LoomAdventureRequest *request)
{
    if (loom_adventure_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (request == (LoomAdventureRequest *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_adventure_state.pending_count == 0u) {
        return LOOM_STATUS_NOT_READY;
    }
    *request = loom_adventure_state.requests[loom_adventure_state.request_head];
    loom_adventure_state.request_head =
        (loom_u8)((loom_adventure_state.request_head + 1u) &
                  (LOOM_ADVENTURE_REQUEST_CAPACITY - 1u));
    --loom_adventure_state.pending_count;
    ++loom_adventure_debug_epoch;
    return LOOM_STATUS_OK;
}

void loom_adventure_debug_snapshot(LoomAdventureDebugSnapshot *snapshot)
{
    snapshot->flags = loom_adventure_state.flags;
    snapshot->request_count = loom_adventure_state.request_count;
    snapshot->last_request_kind = loom_adventure_state.last_request_kind;
    snapshot->last_request_id = loom_adventure_state.last_request_id;
    snapshot->pending_request_count = loom_adventure_state.pending_count;
    snapshot->reserved = 0u;
}

loom_u16 loom_adventure_flags(void)
{
    return loom_adventure_state.flags;
}

loom_u16 loom_adventure_request_count(void)
{
    return loom_adventure_state.request_count;
}

loom_u8 loom_adventure_pending_request_count(void)
{
    return loom_adventure_state.pending_count;
}

loom_u8 loom_adventure_last_request_kind(void)
{
    return loom_adventure_state.last_request_kind;
}

loom_u8 loom_adventure_last_request_id(void)
{
    return loom_adventure_state.last_request_id;
}
