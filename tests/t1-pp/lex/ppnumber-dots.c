// loomcc-do: preprocess
// 1.2.3e+4x is one pp-number: the x inside it is never a macro.
#define x 99
#define e 5
1.2.3e+4x .5e-3 1..2 x
// loomcc-expect: 1.2.3e+4x .5e-3 1..2 99
