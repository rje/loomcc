// loomcc-do: preprocess
#define s(x) #x
s("hi\n") s("a\"b") s("\\")
// loomcc-expect: "\"hi\\n\"" "\"a\\\"b\"" "\"\\\\\""
