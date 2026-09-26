// loomcc-do: preprocess
#define s(x) #x
#define xs(x) s(x)
xs(__FILE__)
// loomcc-expect: "\"stringize-file.c\""
