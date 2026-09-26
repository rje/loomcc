// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
typedef int F(void);
F f(void); // loomcc-error
