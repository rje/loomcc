// loomcc-do: syntax
int g(int a, int b);
int f(int x) { int r = g((x++, x), (x, 2)); if ((r++, r > 0)) return r; for (x = 0, r = 1; x < 3; x++, r *= 2); return r; }
