// loomcc-do: preprocess
// loomcc-note: __FILE__ is the name as found. loomcc spells it like 816-tcc
// loomcc-note: and gcc ("h/file.h"); clang adds "./" when the includer's
// loomcc-note: directory is the current one.
// loomcc-ref-diverges: clang [clang-dot-slash] clang spells it "./h/file.h"
#include "h/file.h"
__FILE__
// loomcc-expect: "h/file.h" "file-in-header.c"
