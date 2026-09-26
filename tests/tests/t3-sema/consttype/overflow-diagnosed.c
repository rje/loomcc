// loomcc-do: syntax
// 6.6p4 (constraint): a constant expression's value must be representable.
// loomcc-ref-diverges: tcc [tcc-no-overflow-diag] 816-tcc folds silently
int a = 32767 + 1; // loomcc-diagnostic: overflow
int b = -32768 - 1; // loomcc-diagnostic: overflow
int c = 200 * 200; // loomcc-diagnostic: overflow
