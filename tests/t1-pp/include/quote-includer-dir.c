// loomcc-do: preprocess
// A "" include searches the directory of the file that contains the #include
// first: sibling.h is found beside nested.h, not beside this file.
#include "h/sub/nested.h"
// loomcc-expect: nested_begin sibling_in_sub nested_end
