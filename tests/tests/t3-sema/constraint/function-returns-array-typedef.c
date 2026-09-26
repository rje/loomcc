// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
typedef int A[3];
A f(void); // loomcc-error
