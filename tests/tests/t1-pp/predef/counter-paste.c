// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-counter] 816-tcc has no __COUNTER__
#define CAT_(a, b) a ## b
#define CAT(a, b) CAT_(a, b)
#define UNIQUE(p) CAT(p, __COUNTER__)
int UNIQUE(v); int UNIQUE(v);
// loomcc-expect: int v0; int v1;
