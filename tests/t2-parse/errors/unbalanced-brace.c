// loomcc-do: syntax
// loomcc-note: Where "expected }" is reported (the open brace, the last
// loomcc-note: line, end of file) varies; any location will do.
void f(void) {
  if (1) {
} // loomcc-error@*
