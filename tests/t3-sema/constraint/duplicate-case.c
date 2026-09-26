// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-switch] 816-tcc accepts duplicate case labels
void f(int x) { switch (x) { case 1: break; case 1: break; } } // loomcc-error
