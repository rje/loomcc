// loomcc-do: preprocess
#define id(x) x
id(id)(id)(3) id(id(id))(4)
// loomcc-expect: id(id)(3) id(4)
