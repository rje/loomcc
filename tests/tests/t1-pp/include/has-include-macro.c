// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-has-include] 816-tcc has no __has_include
#define HDR "h/a.h"
#if __has_include(HDR)
yes
#endif
// loomcc-expect: yes
