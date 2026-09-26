// loomcc-do: preprocess
#define f(x) [x]
#define g(x, y) [x|y]
f() g(,) g(a,) g(,b) g( , )
// loomcc-expect: [] [|] [a|] [|b] [|]
