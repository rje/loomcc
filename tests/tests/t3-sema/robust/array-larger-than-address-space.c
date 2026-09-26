// loomcc-do: syntax
// loomcc-ref:
// The 65816 addresses 16 MiB, so no object can be larger; loomcc tried to
// allocate the object's bytes and aborted (gcc.c-torture pr65680).
struct S { int f : 1; } a[100000000000000001][3]; // loomcc-error
char b[0x1000001]; // loomcc-error
char c[0x10000];
