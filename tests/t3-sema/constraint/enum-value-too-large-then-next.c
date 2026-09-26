// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc does not check enumerator ranges
// 6.7.2.2p2-3 (constraint): the implicit value after LLONG_MAX overflows.
enum big { BIG = 9223372036854775807LL, // loomcc-diagnostic
           NEXT }; // loomcc-diagnostic
