// loomcc-do: preprocess
// Destringizing: \" becomes " and \\ becomes \.
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
_Pragma("weird \"quoted\" \\ text")
// loomcc-expect: #pragma weird "quoted" \ text
