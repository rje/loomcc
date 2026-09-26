// loomcc-do: syntax
// loomcc-note: 816-tcc bit-field allocation (loomcc PLAN section 9): LSB
// loomcc-note: first, units of the declared type, no straddling.
// loomcc-note: Values from scripts/tcc-layout.py.
// loomcc-ref-diverges: clang16 [clang16-bitfield-units] clang packs bit-fields of different declared types into shared bytes
#include "loomcc-test.h"
#include <stddef.h>
struct B { unsigned a:3, b:5, c:9; unsigned char d; };
struct C { unsigned char a:2, b:7; };
struct O { int x : 4; int y : 12; int z : 1; };
struct P { unsigned char a : 4; unsigned b : 4; };
struct Q { unsigned short a : 15; unsigned short b : 2; };
struct R { char c; unsigned x : 1; };
struct S0 { unsigned a : 3; unsigned : 0; unsigned b : 3; };
struct T { unsigned char a : 3; unsigned : 0; unsigned char b : 3; };
STATIC_CHECK(sizeof(struct B) == 6 && offsetof(struct B, d) == 4);
STATIC_CHECK(sizeof(struct C) == 2);
STATIC_CHECK(sizeof(struct O) == 4);
STATIC_CHECK(sizeof(struct P) == 4);
STATIC_CHECK(sizeof(struct Q) == 4);
STATIC_CHECK(sizeof(struct R) == 4);
STATIC_CHECK(sizeof(struct S0) == 4);
STATIC_CHECK(sizeof(struct T) == 2);
