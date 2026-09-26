#include <loom/runtime.h>

typedef struct LoomRuntimeState {
    LoomFrameBoundary boundary;
    LoomInputSnapshot hardware_input;
    const LoomInputSnapshot *accepted_input;
    LoomRuntimeCounters counters;
    loom_u8 initialized;
} LoomRuntimeState;

static LoomRuntimeState loom_runtime_state;

#if defined(LOOM_BUILD_DEBUG)
volatile LoomRuntimeCounters loom_runtime_debug_counters;
/* The status that stopped the tick loop; zero while the game runs. */
volatile loom_u8 loom_runtime_debug_halt_status;
volatile loom_u8 loom_runtime_debug_init_step;
#define LOOM_RUNTIME_PUBLISH_DEBUG_COUNTERS() \
    (loom_runtime_debug_counters = loom_runtime_state.counters)
#else
#define LOOM_RUNTIME_PUBLISH_DEBUG_COUNTERS() ((void)0)
#endif

static loom_u16 loom_runtime_saturating_add(loom_u16 value,
                                            loom_u16 increment)
{
    loom_u16 remaining;

    remaining = (loom_u16)(0xffffu - value);
    if (increment > remaining) {
        return 0xffffu;
    }
    return (loom_u16)(value + increment);
}

#if !defined(__65816__) || defined(LOOM_BUILD_DEBUG)
static LoomStatus loom_runtime_validate_snapshot(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    loom_u8 input_source)
{
    if (boundary->frame_id != input->frame_id ||
        input->pad_count != LOOM_INPUT_PAD_CAPACITY ||
        input->reserved != 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (input_source != LOOM_RUNTIME_INPUT_HARDWARE &&
        input_source != LOOM_RUNTIME_INPUT_REPLAY) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_OK;
}
#endif

static void loom_runtime_accept_boundary(const LoomFrameBoundary *boundary,
                                         loom_u8 input_source)
{
    loom_u16 lag_frames;

    lag_frames = 0u;
    if (boundary->missed_commit_count >=
        loom_runtime_state.counters.missed_frame_count) {
        lag_frames = (loom_u16)(
            boundary->missed_commit_count -
            loom_runtime_state.counters.missed_frame_count);
    }
    loom_runtime_state.counters.frame_id = boundary->frame_id;
    loom_runtime_state.counters.missed_frame_count =
        boundary->missed_commit_count;
    loom_runtime_state.counters.last_lag_frame_count = lag_frames;
    loom_runtime_state.counters.input_source = input_source;
    loom_runtime_state.counters.reserved = 0u;
    if (lag_frames != 0u) {
        loom_runtime_state.counters.lag_event_count =
            loom_runtime_saturating_add(
                loom_runtime_state.counters.lag_event_count, 1u);
    }
}

LoomStatus loom_runtime_initialize(void)
{
    LoomStatus status;

    if (loom_runtime_state.initialized != LOOM_FALSE) {
        return LOOM_STATUS_BUSY;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(1);
    status = loom_port_init();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    LOOM_RUNTIME_DEBUG_INIT_STEP(2);
    status = loom_generated_runtime_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_runtime_state.counters.frame_id = 0u;
    loom_runtime_state.counters.logical_tick_count = 0u;
    loom_runtime_state.counters.missed_frame_count = 0u;
    loom_runtime_state.counters.lag_event_count = 0u;
    loom_runtime_state.counters.last_lag_frame_count = 0u;
    loom_runtime_state.counters.input_source =
        LOOM_RUNTIME_INPUT_HARDWARE;
    loom_runtime_state.counters.reserved = 0u;
    LOOM_RUNTIME_PUBLISH_DEBUG_COUNTERS();
    loom_runtime_state.initialized = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_runtime_tick(void)
{
    LoomStatus status;
    loom_u8 input_source;

    if (loom_runtime_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    status = loom_port_frame_wait(&loom_runtime_state.boundary,
                                  &loom_runtime_state.hardware_input);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    input_source = LOOM_RUNTIME_INPUT_HARDWARE;
#if defined(LOOM_BUILD_DEBUG)
    status = loom_replay_select_input(&loom_runtime_state.boundary,
                                      &loom_runtime_state.hardware_input,
                                      &loom_runtime_state.accepted_input,
                                      &input_source);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#else
    loom_runtime_state.accepted_input = &loom_runtime_state.hardware_input;
#endif
#if !defined(__65816__) || defined(LOOM_BUILD_DEBUG)
    /* A console release build's input is always the adapter's own sample of
     * this frame, so only replays and host ports need the check. */
    status = loom_runtime_validate_snapshot(&loom_runtime_state.boundary,
                                            loom_runtime_state.accepted_input,
                                            input_source);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif

    loom_runtime_accept_boundary(&loom_runtime_state.boundary, input_source);
    status = loom_generated_runtime_tick(
        &loom_runtime_state.boundary,
        loom_runtime_state.accepted_input,
        &loom_runtime_state.counters);
    if (status == LOOM_STATUS_OK) {
        loom_runtime_state.counters.logical_tick_count =
            loom_runtime_saturating_add(
                loom_runtime_state.counters.logical_tick_count, 1u);
    }
    LOOM_RUNTIME_PUBLISH_DEBUG_COUNTERS();
#if defined(LOOM_BUILD_DEBUG)
    loom_replay_observe_tick(&loom_runtime_state.boundary,
                             loom_runtime_state.accepted_input,
                             &loom_runtime_state.counters,
                             status);
#endif
    return status;
}

LoomStatus loom_runtime_read_counters(LoomRuntimeCounters *destination)
{
    if (destination == (LoomRuntimeCounters *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_runtime_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    *destination = loom_runtime_state.counters;
    return LOOM_STATUS_OK;
}

void loom_runtime_main(void)
{
    LoomStatus status;

    /*
     * PVSnesLib's 816-tcc emits one same-named .bss section per translation
     * unit, while its startup clears only one duplicate section. Establish
     * the root sentinel explicitly before the guarded public initializer.
     */
    loom_runtime_state.initialized = LOOM_FALSE;
    status = loom_runtime_initialize();
    while (status == LOOM_STATUS_OK) {
        status = loom_runtime_tick();
    }
#if defined(LOOM_BUILD_DEBUG)
    loom_runtime_debug_halt_status = status;
#endif
    for (;;) {
        /* Fail-stop; a debug build leaves the status above for a watch. */
    }
}
