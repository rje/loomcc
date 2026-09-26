// loomcc-do: run
// loomcc-int: agnostic
// Harness self-test: printed output that differs from the expectation fails.
// loomcc-xfail: deliberately prints the wrong text
// loomcc-ref-diverges: host deliberately prints the wrong text
// loomcc-ref-diverges: host16 deliberately prints the wrong text
// loomcc-ref-diverges: tcc-rom deliberately prints the wrong text
int printf(const char *fmt, ...);
int main(void) { printf("actual\n"); return 0; }
// loomcc-expect-stdout: expected
