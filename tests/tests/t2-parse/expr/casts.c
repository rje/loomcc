// loomcc-do: syntax
int f(long l, void *v) {
  char c = (char)l;
  int *p = (int *)v;
  void (*fp)(void) = (void (*)(void))0;
  (void)fp;
  return (int)(unsigned char)c + (int)(long)p;
}
