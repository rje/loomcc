// loomcc-do: syntax
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-ref: tcc
// loomcc-source: design question Q2 (docs/FINDINGS.md)
// Under Loom's include order (devkitsnes/include on the path), <stdint.h>
// and <stddef.h> must still give 32-bit int32_t and a 16-bit size_t.
#include <snes.h>
#include <loom/runtime.h>
#include <stdint.h>
#include <stddef.h>
typedef char int32_is_4[(sizeof(int32_t) == 4) ? 1 : -1];
typedef char uint32_is_4[(sizeof(uint32_t) == 4) ? 1 : -1];
typedef char int16_is_2[(sizeof(int16_t) == 2) ? 1 : -1];
typedef char size_t_is_2[(sizeof(size_t) == 2) ? 1 : -1];
