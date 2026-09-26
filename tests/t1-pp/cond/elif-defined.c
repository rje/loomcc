// loomcc-do: preprocess
#define B
#if defined A
a
#elif defined B
b
#endif
// loomcc-expect: b
