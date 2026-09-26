// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc misses this declaration constraint
struct S { int a; char a; }; // loomcc-error
