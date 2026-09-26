// loomcc-do: preprocess
#define N 4
#define s(x) #x
#define xs(x) s(x)
s(N) xs(N) s(s(N)) xs(s(N))
// loomcc-expect: "N" "4" "s(N)" "\"N\""
