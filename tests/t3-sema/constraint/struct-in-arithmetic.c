// loomcc-do: syntax
struct S { int a; } s;
int f(void) { return s + 1; } // loomcc-error
