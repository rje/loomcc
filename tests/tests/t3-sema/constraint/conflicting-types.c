// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc misses this declaration constraint
int f(void);
long f(void); // loomcc-error
