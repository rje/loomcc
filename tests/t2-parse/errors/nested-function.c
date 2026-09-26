// loomcc-do: syntax
// Nested function definitions are a GNU extension, not C.
void outer(void) { void inner(void) { } } // loomcc-error
