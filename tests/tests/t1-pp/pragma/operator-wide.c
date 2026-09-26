// loomcc-do: preprocess
// An L prefix is dropped when destringizing.
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
_Pragma(L"wide")
// loomcc-expect: #pragma wide
