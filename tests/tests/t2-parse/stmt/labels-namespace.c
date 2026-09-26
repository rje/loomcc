// loomcc-do: syntax
// Labels have their own name space.
int x(void) { int x = 0; goto x; x: return x; }
