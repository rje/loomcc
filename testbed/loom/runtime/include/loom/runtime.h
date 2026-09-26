#ifndef LOOM_RUNTIME_H
#define LOOM_RUNTIME_H

#include <loom/port.h>

#define LOOM_RUNTIME_CONTRACT_VERSION ((loom_u16)1u)

#define LOOM_RUNTIME_INPUT_HARDWARE ((loom_u8)0u)
#define LOOM_RUNTIME_INPUT_REPLAY ((loom_u8)1u)

/* Stable phase identities used only by compile-time trace instrumentation. */
#define LOOM_RUNTIME_PHASE_READ_ACTIONS ((loom_u8)1u)
#define LOOM_RUNTIME_PHASE_UPDATE_BEHAVIORS ((loom_u8)2u)
#define LOOM_RUNTIME_PHASE_MOVE_AND_COLLIDE ((loom_u8)3u)
#define LOOM_RUNTIME_PHASE_DISPATCH_TRIGGERS ((loom_u8)4u)
#define LOOM_RUNTIME_PHASE_ADVANCE_ANIMATION ((loom_u8)5u)
#define LOOM_RUNTIME_PHASE_BUILD_RENDER ((loom_u8)6u)
#define LOOM_RUNTIME_PHASE_PROCESS_AUDIO ((loom_u8)7u)
#define LOOM_RUNTIME_PHASE_SUBMIT_FRAME ((loom_u8)8u)
#define LOOM_RUNTIME_PHASE_DEBUG_WITNESS ((loom_u8)9u)

/*
 * Generated schedules invoke this macro directly before each selected phase.
 * It compiles away normally; host/debug builds may define it as a function-like
 * macro without installing a console-side callback table.
 */
#if defined(LOOM_BUILD_DEBUG) && defined(__65816__) && \
    !defined(LOOM_RUNTIME_TRACE_PHASE)
/* Debug ROMs record the scanline at every phase boundary in the PVSnesLib
 * adapter, so the Game inspector can report what a tick costs. */
void loom_pvs_debug_phase(loom_u8 phase_id);
#define LOOM_RUNTIME_TRACE_PHASE(phase_id) loom_pvs_debug_phase(phase_id)
#endif
#ifndef LOOM_RUNTIME_TRACE_PHASE
#define LOOM_RUNTIME_TRACE_PHASE(phase_id) ((void)(phase_id))
#endif

/*
 * Finer marks inside a phase for profiling sessions. They cost a scanline
 * each on the console, so they compile away unless a build defines
 * LOOM_RUNTIME_PROFILE_MARKS; the adapter then exposes
 * loom_pvs_debug_phase_cost[16 + n] for the Game inspector's watches.
 */
#if defined(LOOM_RUNTIME_PROFILE_MARKS) && defined(LOOM_BUILD_DEBUG) && \
    defined(__65816__)
void loom_pvs_debug_mark(loom_u8 mark_id);
#define LOOM_RUNTIME_TRACE_MARK(mark_id) loom_pvs_debug_mark(mark_id)
#else
#define LOOM_RUNTIME_TRACE_MARK(mark_id) ((void)(mark_id))
#endif

typedef struct LoomRuntimeCounters {
    LoomFrameId frame_id;
    loom_u16 logical_tick_count;
    loom_u16 missed_frame_count;
    loom_u16 lag_event_count;
    loom_u16 last_lag_frame_count;
    loom_u8 input_source;
    loom_u8 reserved;
} LoomRuntimeCounters;

LOOM_STATIC_ASSERT(loom_runtime_counters_is_twelve_bytes,
                   sizeof(LoomRuntimeCounters) == 12u);

/* Portable frame-shell entry points. */
LoomStatus loom_runtime_initialize(void);
LoomStatus loom_runtime_tick(void);
LoomStatus loom_runtime_read_counters(LoomRuntimeCounters *destination);

/* The explicit debug profile retains a live debugger-readable snapshot. */
#if defined(LOOM_BUILD_DEBUG)
extern volatile LoomRuntimeCounters loom_runtime_debug_counters;
extern volatile loom_u8 loom_runtime_debug_halt_status;
/* The last step entered: 1 port init, 2 the generated initialiser (whose
 * module calls count from 10 up), then each tick phase from 100 up. With
 * the halt status this names the initializer that refused to boot, or the
 * phase a tick never came back from. */
extern volatile loom_u8 loom_runtime_debug_init_step;
#define LOOM_RUNTIME_DEBUG_INIT_STEP(step) \
    (loom_runtime_debug_init_step = (loom_u8)(step))
#else
#define LOOM_RUNTIME_DEBUG_INIT_STEP(step) ((void)0)
#endif

/*
 * Implemented by generated project source. The tick expands to direct calls in
 * the selected profile's top-down order; it is not a runtime scheduler.
 */
LoomStatus loom_generated_runtime_initialize(void);
LoomStatus loom_generated_runtime_tick(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *input,
    const LoomRuntimeCounters *counters);

/* Debug/test replay injection is absent from release cartridge builds. */
#if defined(LOOM_BUILD_DEBUG)
/* Points accepted_input at the hardware snapshot or at a replay record the
 * implementation owns; nothing is copied on the pass-through path. */
LoomStatus loom_replay_select_input(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *hardware_input,
    const LoomInputSnapshot **accepted_input,
    loom_u8 *input_source);
void loom_replay_observe_tick(
    const LoomFrameBoundary *boundary,
    const LoomInputSnapshot *accepted_input,
    const LoomRuntimeCounters *counters,
    LoomStatus tick_status);
#endif

#endif
