// loomcc-do: preprocess
#define X 1
#if 0
#undef X
#define Y 2
#endif
X Y
// loomcc-expect: 1 Y
