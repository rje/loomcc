// loomcc-do: preprocess
// loomcc-options: -Ii1
// loomcc-note: __has_include is C23 (6.10.1); loomcc lists it (PLAN 2.1).
// loomcc-ref-diverges: tcc [tcc-no-has-include] 816-tcc has no __has_include
#if __has_include("h/a.h") && __has_include(<angle.h>) && !__has_include("nope.h") && !__has_include(<nope.h>)
yes
#endif
#ifdef __has_include
defined
#endif
// loomcc-expect: yes defined
