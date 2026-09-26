// loomcc-do: syntax
// loomcc-note: `= {}` is C23 (and GNU); in C17 an initializer list needs one element.
// loomcc-ref-diverges: tcc [tcc-empty-init] 816-tcc accepts `{}`
int a[2] = {}; // loomcc-diagnostic
