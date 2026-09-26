// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-array-static] 816-tcc rejects qualifiers in array parameter declarators (C99)
#include "loomcc-test.h"
int f(int a[], int b[10], int c[][3], int (*d)[3], int e[const 4]);
int f(int *a, int *b, int (*c)[3], int (*d)[3], int *const e) { (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
