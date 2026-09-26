// loomcc-do: preprocess
// Defined-ness does not look at the value.
#define E
#define Z 0
#if defined(E) && defined Z
yes
#endif
// loomcc-expect: yes
