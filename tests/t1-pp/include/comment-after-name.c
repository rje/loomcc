// loomcc-do: preprocess
// loomcc-no-warnings
#include "h/a.h" // a comment is fine
#include /* here too */ "h/a.h" /* and here */
// loomcc-expect: a_h_content a_h_content
