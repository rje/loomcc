// loomcc-do: syntax
int f(int a, int b) { return a ? b ? 1 : 2 : 3; }
int g(int a, int b, int c) { int x; (a ? b : c) + 1; x = a ? b : c; return x; }
