// loomcc-do: preprocess
// Only parentheses group commas; braces and brackets do not.
#define g(x, y) <x|y>
g({1, 2}) g([a, b])
// loomcc-expect: <{1| 2}> <[a| b]>
