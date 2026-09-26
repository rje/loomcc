// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-paste-diag] 816-tcc pastes silently
#define cat(a, b) a ## b
cat(/, /) // loomcc-diagnostic: past
