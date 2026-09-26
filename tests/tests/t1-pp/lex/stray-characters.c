// loomcc-do: preprocess
// A lone ` or @ is a pp-token of its own ("each non-white-space character").
// loomcc-ref-diverges: tcc [tcc-stray] 816-tcc rejects stray characters
@ `
// loomcc-expect: @ `
