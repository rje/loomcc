// loomcc-do: syntax
// 6.7.6.2p1 (constraint): the size shall be greater than zero.
// loomcc-ref-diverges: tcc [tcc-zero-array] 816-tcc accepts [0]
int a[0]; // loomcc-diagnostic
