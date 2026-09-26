// loomcc-do: preprocess
#define str(x) #x
#define xstr(x) str(x)
#define DIR h
#define INC(f) xstr(DIR/f.h)
#include INC(a)
// loomcc-expect: a_h_content
