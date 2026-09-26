// loomcc-do: preprocess
#define cat(a, b) a ## b
cat(L, "x") cat(L, 'y') cat(u, "z")
// loomcc-expect: L"x" L'y' u"z"
