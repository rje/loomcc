/* e_19_3.c:    Redefinitions of macros.    */

/* { dg-do preprocess } */

/* Excerpts from ISO C 3.8.3 "Examples".    */

#define OBJ_LIKE        (1-1)
#define FTN_LIKE(a)     ( a )

/* The following redefinitions should be diagnosed. */

/* 19.3:    */
/* different token sequence     */
#define OBJ_LIKE        (0)     /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */ // loomcc-error

/*  (1-1);  */
    OBJ_LIKE;

/* 19.4:    */
#undef  OBJ_LIKE
#define OBJ_LIKE        (1-1)
/* different white space        */
#define OBJ_LIKE        (1 - 1) /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */ // loomcc-error

/* 19.5:    */
/* different parameter usage    */
#define FTN_LIKE(b)     ( a )   /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */ // loomcc-error

/*  ( x );  */
    FTN_LIKE(x);

/* 19.6:    */
#undef  FTN_LIKE
#define FTN_LIKE(a)     ( a )
/* different parameter spelling */
#define FTN_LIKE(b)     ( b )   /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */ // loomcc-error

/* 19.7:    Not in ISO C "Examples" */
#define FTN_LIKE        OBJ_LIKE    /* { dg-error "redefined\n\[\^ \]*( error:|) this is the location | The macro is redefined" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_19_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_19_3.c:14 (directive line 14)
