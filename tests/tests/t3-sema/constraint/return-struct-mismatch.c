// loomcc-do: syntax
struct A { int x; };
struct B { int x; } b;
struct A f(void) { return b; } // loomcc-error
