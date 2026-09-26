// loomcc-do: preprocess
// loomcc-options: -Ih
// The includer's directory comes before -I for "" includes.
#include "decoy.h"
#include <decoy.h>
// loomcc-expect: decoy_in_test_dir decoy_in_h_dir
