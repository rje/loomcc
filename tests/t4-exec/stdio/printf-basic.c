// loomcc-do: run
// loomcc-int: agnostic
// The harness's printf (harness/rom/stdio.c in ROMs; the C library on hosts).
int printf(const char *fmt, ...);
int puts(const char *s);
int putchar(int c);
int main(void) {
  short n = -42;
  unsigned short u = 65535u;
  printf("hello %d %u %x %X %c %s|\n", n, u, 0xbeef, 0xbeef, 'Z', "str");
  printf("[%5d][%-5d][%05d][%3s][%-3s]\n", 42, 42, 42, "a", "b");
  puts("puts line");
  putchar('!');
  putchar('\n');
  printf("%% done\n");
  return 0;
}
// loomcc-expect-stdout: hello -42 65535 beef BEEF Z str|
// loomcc-expect-stdout: [   42][42   ][00042][  a][b  ]
// loomcc-expect-stdout: puts line
// loomcc-expect-stdout: !
// loomcc-expect-stdout: % done
