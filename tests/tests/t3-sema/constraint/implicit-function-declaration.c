// loomcc-do: syntax
// No implicit function declarations since C99.
// loomcc-ref-diverges: tcc [tcc-implicit-function] 816-tcc declares it implicitly
int f(void) { return undeclared_function(1); } // loomcc-diagnostic
