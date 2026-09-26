// loomcc-do: preprocess
#define f() 1
f() f( ) f(/* c */) f
// loomcc-expect: 1 1 1 f
