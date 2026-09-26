// loomcc-do: preprocess
// loomcc-source: Loom runtime/include/loom/loom_types.h (LOOM_STATIC_ASSERT pattern)
#define LOOM_STATIC_ASSERT_CAT_(a, b) a##b
#define LOOM_STATIC_ASSERT_CAT(a, b) LOOM_STATIC_ASSERT_CAT_(a, b)
#define LOOM_STATIC_ASSERT(expr, name) \
    typedef char LOOM_STATIC_ASSERT_CAT(loom_static_assert_, name)[(expr) ? 1 : -1]
LOOM_STATIC_ASSERT(sizeof(int) == 2, int_is_16_bits);
// loomcc-expect: typedef char loom_static_assert_int_is_16_bits[(sizeof(int) == 2) ? 1 : -1];
