// loomcc-do: preprocess
// Standard C: `, ## <empty>` pastes a comma with a placemarker, keeping the comma.
// loomcc-ref-diverges: tcc [tcc-gnu-comma] 816-tcc applies the GNU comma swallow
#define e(fmt, ...) fn(fmt, ## __VA_ARGS__)
e(x,)
// loomcc-expect: fn(x,)
