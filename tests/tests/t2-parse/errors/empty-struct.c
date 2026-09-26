// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
// A struct needs at least one named member in C (empty structs are GNU).
struct Empty { }; // loomcc-diagnostic
