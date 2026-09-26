// loomcc-do: preprocess
#define cat(a, b) a ## b
cat(u8, "s")
// loomcc-expect: u8"s"
