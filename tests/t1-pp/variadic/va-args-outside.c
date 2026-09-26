// loomcc-do: preprocess
// C17 6.10.3p5: __VA_ARGS__ only in the replacement list of a variadic macro.
// loomcc-ref-diverges: tcc [tcc-va-args-anywhere] 816-tcc accepts __VA_ARGS__ anywhere
#define f(x) __VA_ARGS__ // loomcc-diagnostic: __VA_ARGS__
