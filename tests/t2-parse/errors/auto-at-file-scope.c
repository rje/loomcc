// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
// 6.9p2 (constraint): no auto or register at file scope.
auto int x; // loomcc-error
