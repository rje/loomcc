// loomcc-do: syntax
struct A { int x; } a;
struct B { int x; } b;
void f(void) { a = b; } // loomcc-error
