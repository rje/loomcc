// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: A self-test of the harness: returning non-zero is a failure.
// loomcc-xfail: deliberately returns 3
// loomcc-ref-diverges: host deliberately returns 3
// loomcc-ref-diverges: host16 deliberately returns 3
// loomcc-ref-diverges: tcc-rom deliberately returns 3
int main(void) { return 3; }
