// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-switch] 816-tcc accepts duplicate case labels
enum E { A = 3, B = 3 };
void f(int x) { switch (x) { case A: break; case B: break; } } // loomcc-error
