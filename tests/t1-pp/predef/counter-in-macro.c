// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-counter] 816-tcc has no __COUNTER__
#define C __COUNTER__
#define TWICE C C
TWICE C
#if __COUNTER__ == 3
yes
#endif
__COUNTER__
// loomcc-expect: 0 1 2 yes 4
