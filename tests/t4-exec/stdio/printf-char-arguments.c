// loomcc-do: run
// loomcc-int: 16
// Arguments of type char passed to a variadic function undergo the default
// argument promotions (6.5.2.2p7): they arrive as int.
// loomcc-ref-diverges: tcc-rom [tcc-vararg-char] 816-tcc pushes a char variadic argument as one byte
int printf(const char *fmt, ...);
static unsigned short three(void) { return 3; }
int main(void) {
  unsigned char a = 1, b = 2;
  signed char c = -5;
  printf("%u %u %u %d\n", a, b, three(), c);
  return 0;
}
// loomcc-expect-stdout: 1 2 3 -5
