// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-missing-endif-eof] 816-tcc reports a missing #endif at the end of the file
// An #if in a header must end in that header.
#include "inc/unterminated-if.h" // loomcc-error@inc/unterminated-if.h:2
