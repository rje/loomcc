// loomcc-do: preprocess
#define s(x) #x
#define xs(x) s(x)
xs(__LINE__) s(__LINE__)
// loomcc-expect: "4" "__LINE__"
