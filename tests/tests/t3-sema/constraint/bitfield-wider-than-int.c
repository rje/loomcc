// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc misses this declaration constraint
// 6.7.2.1p4 (constraint): the width may not exceed the type's width (16 bits).
unsigned wide : 17; // loomcc-error
struct S { unsigned too_wide : 17; }; // loomcc-error
