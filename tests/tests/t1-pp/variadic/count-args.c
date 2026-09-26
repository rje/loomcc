// loomcc-do: preprocess
#define CNT(a, b, c, d, e, ...) e
#define COUNT(...) CNT(__VA_ARGS__, 4, 3, 2, 1, 0)
COUNT(x) COUNT(x, y) COUNT(x, y, z) COUNT(x, y, z, w)
// loomcc-expect: 1 2 3 4
