// loomcc-do: preprocess
// C23 6.10.1p?: __has_include may appear only in #if/#elif (and defined).
// loomcc-ref-diverges: tcc [tcc-no-has-include] 816-tcc has no __has_include
int x = __has_include("h/a.h"); // loomcc-error
