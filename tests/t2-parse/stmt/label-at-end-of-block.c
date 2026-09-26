// loomcc-do: syntax
// loomcc-note: A label must be followed by a statement in C17 (C23 relaxes it).
void f(void) { { goto end; end: } } // loomcc-diagnostic
