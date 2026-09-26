/* Q2: under Loom's include order, <stdint.h>/<stddef.h> must be loomcc's. */
#include <snes.h>
#include <loom/runtime.h>
#include <stdint.h>
#include <stddef.h>
typedef char q2_int32_is_4[(sizeof(int32_t) == 4) ? 1 : -1];
typedef char q2_uint32_is_4[(sizeof(uint32_t) == 4) ? 1 : -1];
typedef char q2_int16_is_2[(sizeof(int16_t) == 2) ? 1 : -1];
typedef char q2_size_t_is_2[(sizeof(size_t) == 2) ? 1 : -1];
