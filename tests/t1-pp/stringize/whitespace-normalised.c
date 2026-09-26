// loomcc-do: preprocess
#define s(x) #x
s(  a   +   b  ) s(a+b) s(	a	)
// loomcc-expect: "a + b" "a+b" "a"
