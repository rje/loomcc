// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: A self-test of the harness: this test must fail everywhere.
// loomcc-xfail: deliberately calls abort()
// loomcc-ref-diverges: host deliberately calls abort()
// loomcc-ref-diverges: host16 deliberately calls abort()
// loomcc-ref-diverges: tcc-rom deliberately calls abort()
void abort(void);
int main(void) { abort(); return 0; }
