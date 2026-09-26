// loomcc-do: syntax
// 6.2.1p4: a tag declared in the parameter list of a function definition
// has block scope ending with the function body, so `struct S` is complete
// inside it (clang and gcc warn that the type is not visible outside).
// Found through gcc.dg struct-in-proto-1.c.
int f(struct S { int i; } s) {
  return (int)sizeof(struct S) + s.i;
}
