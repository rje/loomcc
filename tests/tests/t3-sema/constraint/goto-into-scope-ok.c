// loomcc-do: syntax
// Jumping past a (non-VLA) declaration into its scope is allowed.
// loomcc-no-warnings
int f(int a) { if (a) goto in; { int x = 1; in: x = 2; return x; } }
