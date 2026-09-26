// loomcc-do: preprocess
#define s(x) #x
s(abc) s(123) s(a b)
// loomcc-expect: "abc" "123" "a b"
