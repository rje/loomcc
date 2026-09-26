// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-line-after] 816-tcc reports an undefined label at the end of the function
void f(void) { goto nowhere; } // loomcc-error
