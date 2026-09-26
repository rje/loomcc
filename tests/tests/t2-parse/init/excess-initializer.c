// loomcc-do: syntax
// 6.7.9p2 (constraint): no initializer for an object outside the entity.
// clang reports excess elements as a warning.
// loomcc-ref-diverges: tcc [tcc-lax-init] 816-tcc ignores excess initializers
struct P { int x, y, z; };
struct P q = { .z = 3, 4 }; // loomcc-diagnostic
int a[2] = { 1, 2, 3 }; // loomcc-diagnostic
