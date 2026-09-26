// loomcc-do: preprocess
#define s(x) #x
s() s( ) s(/* c */)
// loomcc-expect: "" "" ""
