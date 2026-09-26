// loomcc-do: preprocess
// C17 6.10.3p3: an object-like macro needs whitespace after its name.
// loomcc-ref-diverges: tcc [tcc-no-c99-ws] 816-tcc accepts it silently
#define X-1 // loomcc-diagnostic: whitespace
X
// loomcc-expect: -1
