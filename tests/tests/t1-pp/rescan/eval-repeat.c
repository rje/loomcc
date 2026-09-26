// loomcc-do: preprocess
// loomcc-note: EVAL-driven recursion (the same rule as defer-recursion.c): a
// loomcc-note: countdown that needs names to stay unpainted across EVAL passes.
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
#define EVAL(...) EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) __VA_ARGS__
#define CAT(a, ...) a ## __VA_ARGS__
#define DEC(x) CAT(DEC_, x)
#define DEC_0 0
#define DEC_1 0
#define DEC_2 1
#define DEC_3 2
#define DEC_4 3
#define IF(c) CAT(IF_, c)
#define IF_0(t, ...) __VA_ARGS__
#define IF_1(t, ...) t
#define BOOL(x) CAT(BOOL_, x)
#define BOOL_0 0
#define BOOL_1 1
#define BOOL_2 1
#define BOOL_3 1
#define BOOL_4 1
#define COUNTDOWN(n) n IF(BOOL(n))(OBSTRUCT(COUNTDOWN_INDIRECT)()(DEC(n)), )
#define COUNTDOWN_INDIRECT() COUNTDOWN
EVAL(COUNTDOWN(4))
// loomcc-expect: 4 3 2 1 0
