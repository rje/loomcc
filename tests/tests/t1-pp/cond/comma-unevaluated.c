// loomcc-do: preprocess
// loomcc-no-warnings
// A comma inside an unevaluated operand is allowed.
#if 1 || (1, 2)
yes
#endif
// loomcc-expect: yes
