// loomcc-do: syntax
// loomcc-note: at file scope a non-constant size is a constraint violation
// loomcc-note: (6.7.6.2p2: a VLA must have block scope).
int n = 3;
int a[n]; // loomcc-error
