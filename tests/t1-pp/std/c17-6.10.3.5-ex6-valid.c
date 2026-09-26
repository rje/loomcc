// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p8 EXAMPLE 6: the valid redefinitions.
// loomcc-no-warnings
#define OBJ_LIKE (1-1)
#define OBJ_LIKE /* white space */ (1-1) /* other */
#define FUNC_LIKE(a) ( a )
#define FUNC_LIKE( a )( /* note the white space */ \
 a /* other stuff on this line
 */ )
OBJ_LIKE FUNC_LIKE(z)
// loomcc-expect: (1-1) ( z )
