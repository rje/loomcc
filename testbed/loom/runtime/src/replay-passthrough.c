#include <loom/runtime.h>

LoomStatus loom_replay_select_input(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *hardware_input,
    const LoomInputSnapshot **accepted_input,
    loom_u8 *input_source)
{
    if (boundary == (const LoomFrameBoundary *)0 ||
        hardware_input == (const LoomInputSnapshot *)0 ||
        accepted_input == (const LoomInputSnapshot **)0 ||
        input_source == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    *accepted_input = hardware_input;
    *input_source = LOOM_RUNTIME_INPUT_HARDWARE;
    return LOOM_STATUS_OK;
}

void loom_replay_observe_tick(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *accepted_input,
    const LoomRuntimeCounters *counters,
    LoomStatus tick_status)
{
    (void)boundary;
    (void)accepted_input;
    (void)counters;
    (void)tick_status;
}
