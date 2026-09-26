// loomcc-do: syntax
int f(int n) {
  int s = 0;
a: s++;
b: if (s < n) goto a;
c: while (0) ;
d: { }
e: return s;
}
