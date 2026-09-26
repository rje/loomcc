// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
// 6.7.3p2 (constraint): restrict needs a pointer to an object type.
restrict int x; // loomcc-error
