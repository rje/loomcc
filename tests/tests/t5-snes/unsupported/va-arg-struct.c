// loomcc-do: compile
// loomcc-ref:
// Variadic functions are not supported by the 65816 backend: va_arg of a
// struct type is an error, not endless recursion between the aggregate and
// lvalue lowerings (gcc.c-torture 931004-2.c overflowed the stack).
#include <stdarg.h>
struct tiny { int c; };
int f(int n, ...) {
  va_list ap;
  va_start(ap, n); // loomcc-error
  struct tiny x = va_arg(ap, struct tiny); // loomcc-error
  va_end(ap); // loomcc-error
  return x.c;
}
