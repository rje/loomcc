// loomcc-do: syntax
// The implementation's own <stdint.h>: exact-width types have their widths.
#include <stdint.h>
#include "loomcc-test.h"
STATIC_CHECK(sizeof(int8_t) == 1 && sizeof(uint8_t) == 1);
STATIC_CHECK(sizeof(int16_t) == 2 && sizeof(uint16_t) == 2);
STATIC_CHECK(sizeof(int32_t) == 4 && sizeof(uint32_t) == 4);
STATIC_CHECK((int16_t)-1 < 0 && (uint16_t)-1 > 0);
STATIC_CHECK((int32_t)-1 < 0 && (uint32_t)-1 > 0);
