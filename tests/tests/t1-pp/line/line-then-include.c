// loomcc-do: preprocess
// loomcc-ref-diverges: clang [clang-dot-slash] clang spells the header "./../include/h/file.h"
// #line changes this file's presumed name; the header keeps its own.
#line 10 "x.c"
#include "../include/h/file.h"
__FILE__ __LINE__
// loomcc-expect: "../include/h/file.h" "x.c" 11
