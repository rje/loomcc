// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-once] 816-tcc ignores #pragma once
// loomcc-note: #pragma once is not standard, but every compiler here honours it.
#include "h/once.h"
#include "h/once.h"
// loomcc-expect: once_content
