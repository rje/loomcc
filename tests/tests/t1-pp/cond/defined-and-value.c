// loomcc-do: preprocess
// The && short-circuits in value only; X is simply 0 when undefined.
#if defined(X) && X > 3
wrong
#else
right
#endif
// loomcc-expect: right
