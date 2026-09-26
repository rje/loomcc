// loomcc-do: syntax
// 6.5.6p2 (constraint): pointer arithmetic needs a complete object type.
// loomcc-ref-diverges: tcc [tcc-void-arith] 816-tcc does void * arithmetic (GNU)
void *f(void *p) { return p + 1; } // loomcc-diagnostic
