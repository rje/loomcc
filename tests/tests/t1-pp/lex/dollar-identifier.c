// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-dollar] 816-tcc does not accept $ in identifiers
// loomcc-note: `$` in identifiers is implementation-defined; clang, gcc and
// loomcc-note: 816-tcc all accept it, and WLA-DX-facing names sometimes use it.
#define a$b 1
a$b $x
// loomcc-expect: 1 $x
