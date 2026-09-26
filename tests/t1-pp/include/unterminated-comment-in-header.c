// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-line-after] 816-tcc reports an unterminated comment at the end of the file
// A comment cannot run from a header into the includer.
#include "h/unterminated-comment.h" // loomcc-error@h/unterminated-comment.h:2: (unterminated|comment)
int after;
