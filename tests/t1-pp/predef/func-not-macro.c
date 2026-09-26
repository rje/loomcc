// loomcc-do: preprocess
// __func__ is an identifier handled by the compiler proper, not a macro.
__func__
#ifdef __func__
wrong
#endif
// loomcc-expect: __func__
