// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-unterminated-invocation] 816-tcc reports an unterminated invocation at EOF as too many arguments
#define f(a) a
f(1, // loomcc-error: (unterminated|end of file|EOF)
