// loomcc-do: preprocess
// A header without a final newline cannot glue its last token to ours.
#include "h/no-newline.h"
;
// loomcc-expect: int no_newline ;
