// loomcc-do: preprocess
// loomcc-options: -iquote i2 -Ii1
// loomcc-ref: clang
// -iquote directories serve "" includes only, ahead of -I.
#include "both.h"
#include <both.h>
// loomcc-expect: both_from_i2 both_from_i1
