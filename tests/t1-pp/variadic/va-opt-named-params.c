// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define LOG(fmt, ...) log(fmt __VA_OPT__(, __VA_ARGS__))
LOG("a") LOG("b", 1) LOG("c", 1, 2)
// loomcc-expect: log("a") log("b", 1) log("c", 1, 2)
