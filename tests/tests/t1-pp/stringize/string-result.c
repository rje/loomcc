// loomcc-do: preprocess
#define s(x) #x
#define xs(x) s(x)
#define STR "a"
xs(STR) xs("b" "c")
// loomcc-expect: "\"a\"" "\"b\" \"c\""
