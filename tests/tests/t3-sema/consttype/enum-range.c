// loomcc-do: syntax
// 6.7.2.2p2 (constraint): an enumerator value must be representable as int.
// loomcc-ref-diverges: tcc [tcc-no-overflow-diag] 816-tcc accepts it
enum { OK = 32767, TOO_BIG = 40000 }; // loomcc-diagnostic
