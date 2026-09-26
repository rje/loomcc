// loomcc-do: run
// loomcc-int: 16
// %ld prints loomcc's 32-bit long.
// loomcc-ref-diverges: tcc-rom [tcc-long16] 816-tcc's long is 16 bits
int printf(const char *fmt, ...);
int main(void) {
  printf("%ld %lu %lx %d\n", -100000L, 4000000000UL, 0x12345678UL, 7);
  return 0;
}
// loomcc-expect-stdout: -100000 4000000000 12345678 7
