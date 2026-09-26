// loomcc-do: preprocess
// loomcc-no-warnings
// After a group has been taken, later #elif expressions are not evaluated.
#if 1
taken
#elif 1/0
#elif garbage (
#endif
// loomcc-expect: taken
