// loomcc-do: syntax
int g;
int f(int *p) {
  int a = -g, b = +g, c = !g, d = ~g, e = *p, *q = &g;
  ++g; --g; g++; g--;
  return a + b + c + d + e + *q + sizeof g + sizeof(int) + sizeof *p + sizeof(g + 1);
}
