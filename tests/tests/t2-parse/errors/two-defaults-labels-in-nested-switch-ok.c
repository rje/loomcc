// loomcc-do: syntax
// Each switch has its own default: nesting is fine.
int f(int a, int b) {
  switch (a) { default: switch (b) { default: return 1; } }
}
