// loomcc-do: preprocess
// Spaces inside the argument are kept only where the source had them.
#define s(x) #x
s(a (b) c) s(a(b)c) s( x ; )
// loomcc-expect: "a (b) c" "a(b)c" "x ;"
