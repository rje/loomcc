// loomcc-do: preprocess
// loomcc-note: C17 6.10.9p2 EXAMPLE (see also pragma/operator-in-macro.c).
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
#define LISTING(x) PRAGMA(listing on #x)
#define PRAGMA(x) _Pragma(#x)
LISTING ( ..\listing.dir )
// loomcc-expect: #pragma listing on "..\listing.dir"
