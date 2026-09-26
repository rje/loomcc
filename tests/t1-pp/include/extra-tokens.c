// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-extra-tokens] 816-tcc ignores tokens after the header name
#include "h/a.h" junk // loomcc-diagnostic
// loomcc-expect: a_h_content
