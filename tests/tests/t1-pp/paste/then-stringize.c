// loomcc-do: preprocess
#define s(x) #x
#define xs(x) s(x)
#define p(a, b) xs(a ## b)
p(x, y) p(1, 2) p(, z)
// loomcc-expect: "xy" "12" "z"
