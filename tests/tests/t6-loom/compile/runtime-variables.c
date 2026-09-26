// loomcc-do: compile
// loomcc-source: Loom d88b68b runtime/src/variables.c (see ../loom-d88b68b/PROVENANCE)
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-ref: tcc
// Stage 1 of T6: the unit compiles with Loom's include paths and debug
// definitions, and the output assembles.
#include "../loom-d88b68b/runtime/src/variables.c"
