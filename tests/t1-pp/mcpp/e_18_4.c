/* e_18_4.c:    #define syntax errors.  */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic" } */

/* 18.4:    Not an identifier.  */
#define "string"    /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */ // loomcc-error
#define 123         /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */ // loomcc-error

/* 18.5:    No argument.    */
#define     /* { dg-error "no macro name given| No identifier" } */ // loomcc-error

/* 18.6:    Empty parameter list.   */
#define math( op, a, )      op( (a), (b))   /* { dg-error "parameter name missing| badly punctuated parameter list| Empty parameter" } */ // loomcc-error

/* 18.7:    Duplicate parameter names.  */
#define math( op, a, a)     op( (a), (b))   /* { dg-error "duplicate macro parameter| duplicate argument name| Duplicate parameter name" } */ // loomcc-error

/* 18.8:    Argument is not an identifier.  */
#define NUMARGS( 1, +, 2)   (1 + 2)     /* { dg-error "may not appear in macro parameter list| invalid character in macro parameter name| Illegal parameter" } */ // loomcc-error

/* 18.9:    No space between macro name and replacement text.   */
/*
    C90 (Corrigendum 1) forbids this if and only the replacement text begins
        with a non-basic-character.
    C99 forbids this even when the replacement text begins with basic-
        character.
*/
/*  From ISO 9899:1990 / Corrigendum 1. */
#define THIS$AND$THAT(a, b)     ((a) + (b))     /* { dg-error "ISO C requires whitespace after the macro name| '\\$' in identifier| No space between macro name" } */
/* Note: the following definition is legal (object-like macro).
#define THIS $AND$THAT(a, b)    ((a) + (b))
*/

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_18_4.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: line 30 is not an error here: `$` is an identifier character for loomcc (implementation-defined)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_18_4.c:8 (directive line 8)
