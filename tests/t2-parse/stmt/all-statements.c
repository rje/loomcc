// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-for-decl] 816-tcc rejects declarations in for (C99)
int f(int n) {
  int s = 0;
  ;
  {}
  if (n) s = 1;
  if (n) s = 1; else s = 2;
  if (n) { if (s) s++; else s--; }
  while (n--) s++;
  do s--; while (s > 10);
  for (;;) break;
  for (int i = 0; i < 3; i++) continue;
  for (s = 0; s < 3; ) s++;
  switch (n) { case 1: s = 1; break; case 2: case 3: s = 2; default: s = 0; }
  switch (n) default: s = 9;
  goto out;
out:
  return s;
}
void g(void) { return; }
