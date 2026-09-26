// loomcc-do: syntax
_Static_assert(1, "file scope");
struct S { int a; _Static_assert(sizeof(int) >= 2, "in a struct"); };
void f(void) { _Static_assert(2 > 1, "block scope"); }
// loomcc-ref-diverges: tcc [tcc-no-static-assert] 816-tcc has no _Static_assert
