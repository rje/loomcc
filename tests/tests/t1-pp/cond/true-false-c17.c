// loomcc-do: preprocess
// In C17, `true` and `false` in #if are identifiers, so both are 0.
#if true
wrong
#elif false
wrong
#else
right
#endif
// loomcc-expect: right
