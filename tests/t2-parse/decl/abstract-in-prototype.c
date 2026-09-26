// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-array-static] 816-tcc rejects static in array parameter declarators (C99)
int f(int, char *, int (*)(int), int [], int (*)[3], void (*)(void));
int g(int (int)); /* a function parameter adjusts to a pointer */
int h(int x[static 3]);
