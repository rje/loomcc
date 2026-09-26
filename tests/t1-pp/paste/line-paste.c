// loomcc-do: preprocess
// __LINE__ must expand before pasting only through an extra level.
#define cat(a, b) a ## b
#define xcat(a, b) cat(a, b)
cat(v, __LINE__) xcat(v, __LINE__)
// loomcc-expect: v__LINE__ v5
