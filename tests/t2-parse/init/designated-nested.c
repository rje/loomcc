// loomcc-do: syntax
struct In { int a[3]; };
struct Out { struct In in; int k; };
struct Out o = { .in.a[1] = 5, .k = 2, .in = { .a = { [2] = 7 } } };
struct Out arr[2] = { [1].in.a[0] = 1, [0] = { .k = 3 } };
