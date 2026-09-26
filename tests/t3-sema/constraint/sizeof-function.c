// loomcc-do: syntax
// 6.5.3.4p1 (constraint): sizeof of a function type.
// loomcc-ref-diverges: tcc [tcc-lax-sizeof] 816-tcc accepts sizeof of a function
int g(void);
int f(void) { return sizeof(g); } // loomcc-diagnostic
