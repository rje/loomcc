// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc misses this declaration constraint
int a = 1;
int a = 2; // loomcc-error
