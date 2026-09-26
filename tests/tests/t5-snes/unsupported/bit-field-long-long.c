// loomcc-do: compile
// loomcc-ref:
// 64-bit types are not supported by the 65816 backend: a long long
// bit-field wider than 32 bits is an error, not a shift-count underflow
// (gcc.c-torture 920501-3.c and bitfld-3.c panicked).
struct S { long long f : 40; } s;
long g(void) { return (long)s.f; } // loomcc-error
