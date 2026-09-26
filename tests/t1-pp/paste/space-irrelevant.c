// loomcc-do: preprocess
#define f(x, y) x    ##    y
#define g(x, y) x##y
f( a , b ) g(a,b)
// loomcc-expect: ab ab
