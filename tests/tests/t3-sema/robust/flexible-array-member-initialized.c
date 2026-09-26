// loomcc-do: syntax
// loomcc-ref:
// Initialising a flexible array member is a GNU extension loomcc does not
// support: an error, not a write past the object (gcc.dg pr56078 panicked).
struct T { int a; char b[]; };
struct T t1 = { .a = 1, .b = { 'a', 'b' } }; // loomcc-error
struct T t2 = { .a = 1 };
