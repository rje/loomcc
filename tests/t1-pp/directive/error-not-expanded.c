// loomcc-do: preprocess
// The #error tokens are not macro-expanded.
#define M expanded
#error M here // loomcc-error: M here
