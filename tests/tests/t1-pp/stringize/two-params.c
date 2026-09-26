// loomcc-do: preprocess
#define s2(a, b) #a #b #a
s2(x, y z)
// loomcc-expect: "x" "y z" "x"
