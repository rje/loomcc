// loomcc-do: syntax
// loomcc-note: In C17 a label must label a statement, not a declaration.
void f(void) { goto l; l: int x = 1; (void)x; } // loomcc-diagnostic
