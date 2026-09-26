/* loomcc-test.h: shared helpers for the loomcc torture suite.
 *
 * Every tool (loomcc, clang, clang --target=msp430, 816-tcc) gets
 * -I harness/include, so tests may include this. It deliberately needs no
 * libc header: execute tests stop with abort() or return non-zero.
 */
#ifndef LOOMCC_TEST_H
#define LOOMCC_TEST_H

void abort(void);

/* Fixed-width types. loomcc: long is 32 bits. 816-tcc: long is 16 bits and
 * long long is 32. Hosts: int is 32 bits. */
typedef signed char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
#if defined(__TINYC__) || defined(__816TCC__) || defined(LOOMCC_TCC)
typedef long long i32;
typedef unsigned long long u32;
#elif defined(__MSP430__) || defined(__loomcc__)
typedef long i32;
typedef unsigned long u32;
#else
typedef int i32;
typedef unsigned int u32;
#endif

/* A static assertion every compiler here understands (816-tcc has no
 * _Static_assert): a negative array size is a constraint violation. */
#define LT_CAT_(a, b) a##b
#define LT_CAT(a, b) LT_CAT_(a, b)
#define STATIC_CHECK(e) typedef char LT_CAT(loomcc_static_check_, __LINE__)[(e) ? 1 : -1]

/* Runtime check: abort() when false. */
#define CHECK(e) do { if (!(e)) abort(); } while (0)

/* Keeps the optimiser from folding a value (all compilers here respect
 * volatile). */
#define OPAQUE(T, v) (*(volatile T *)&(T){v})

#endif
