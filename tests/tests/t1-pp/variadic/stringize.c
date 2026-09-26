// loomcc-do: preprocess
#define s(...) #__VA_ARGS__
s(a , b,c) s() s( x ,  y )
// loomcc-expect: "a , b,c" "" "x , y"
