// loomcc-do: preprocess
// #include is textual: it can land anywhere, even inside a declaration.
int f(void) {
#include "h/a.h"
}
// loomcc-expect: int f(void) { a_h_content }
