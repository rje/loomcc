// loomcc-do: preprocess
// loomcc-ref:
// A #line number past 2147483647 is diagnosed; later locations must not
// overflow (loomcc panicked computing the next line's logical number; found
// by sweeping gcc.dg cpp/line6.c).
#line 18446744073709551616 // loomcc-warning
x
// loomcc-expect: x
