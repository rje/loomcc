// loomcc-do: preprocess
#define s(x) #x
#define xs(x) s(x)
#define cat(a, b) a ## b
xs(cat(x, y)) xs(cat(1, 2))
// loomcc-expect: "xy" "12"
