// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p8 EXAMPLE 6: the invalid redefinitions.
// loomcc-ref-diverges: tcc [tcc-no-redef-diag] 816-tcc accepts incompatible redefinitions
#define OBJ_LIKE (1-1)
#define FUNC_LIKE(a) ( a )
#define OBJ_LIKE (0) // loomcc-diagnostic
#define OBJ_LIKE (1 - 1) // loomcc-diagnostic
#define FUNC_LIKE(b) ( a ) // loomcc-diagnostic
#define FUNC_LIKE(b) ( b ) // loomcc-diagnostic
