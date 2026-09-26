// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-sizeof] 816-tcc accepts sizeof of a bit-field
struct S { unsigned b : 3; };
int f(struct S *s) { return sizeof(s->b); } // loomcc-error
