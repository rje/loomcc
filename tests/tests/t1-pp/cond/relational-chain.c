// loomcc-do: preprocess
#if 3 > 2 > 1
wrong
#else
right
#endif
#if 1 < 2 < 3 && 1 == 1 == 1 && (2 != 3) == 1
yes
#endif
// loomcc-expect: right yes
